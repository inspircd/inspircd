/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Oliver Lupton <oliverlupton@gmail.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "u_listmode.h"

/* $ModDesc: Allows an extended ban (+b) syntax redirecting banned users to another channel */

/* Originally written by Om, January 2009
 */

class BanRedirectEntry
{
 public:
	std::string targetchan;
	std::string banmask;

	BanRedirectEntry(const std::string &target = "", const std::string &mask = "")
	: targetchan(target), banmask(mask)
	{
	}
};

typedef std::vector<BanRedirectEntry> BanRedirectList;
typedef std::deque<std::string> StringDeque;

class BanRedirect : public ModeWatcher
{
 public:
	SimpleExtItem<BanRedirectList> extItem;
	BanRedirect(Module* parent) : ModeWatcher(parent, 'b', MODETYPE_CHANNEL),
		extItem("banredirect", parent)
	{
	}

	bool BeforeMode(User* source, User* dest, Channel* channel, std::string &param, bool adding, ModeType type)
	{
		/* nick!ident@host -> nick!ident@host
		 * nick!ident@host#chan -> nick!ident@host#chan
		 * nick@host#chan -> nick!*@host#chan
		 * nick!ident#chan -> nick!ident@*#chan
		 * nick#chan -> nick!*@*#chan
		 */

		if(channel && (type == MODETYPE_CHANNEL) && param.length())
		{
			BanRedirectList* redirects;

			std::string mask[4];
			enum { NICK, IDENT, HOST, CHAN } current = NICK;
			std::string::iterator start_pos = param.begin();
			long maxbans = channel->GetMaxBans();

			if (param.length() >= 2 && param[1] == ':')
				return true;

			if (param.find('#') == std::string::npos)
				return true;

			if(adding && (channel->bans.size() > static_cast<unsigned>(maxbans)))
			{
				source->WriteNumeric(478, "%s %s %c :Channel ban list for %s is full (maximum entries for this channel is %ld)",
					source->nick.c_str(), channel->name.c_str(), mode, channel->name.c_str(), maxbans);
				return false;
			}

			for(std::string::iterator curr = start_pos; curr != param.end(); curr++)
			{
				switch(*curr)
				{
					case '!':
						mask[current].assign(start_pos, curr);
						current = IDENT;
						start_pos = curr+1;
						break;
					case '@':
						mask[current].assign(start_pos, curr);
						current = HOST;
						start_pos = curr+1;
						break;
					case '#':
						/* bug #921: don't barf when redirecting to ## channels */
						if (current != CHAN)
						{
							mask[current].assign(start_pos, curr);
							current = CHAN;
							start_pos = curr;
						}
						break;
				}
			}

			if(mask[current].empty())
			{
				mask[current].assign(start_pos, param.end());
			}

			/* nick@host wants to be changed to *!nick@host rather than nick!*@host... */
			if(mask[NICK].length() && mask[HOST].length() && mask[IDENT].empty())
			{
				/* std::string::swap() is fast - it runs in constant time */
				mask[NICK].swap(mask[IDENT]);
			}

			if (!mask[NICK].empty() && mask[IDENT].empty() && mask[HOST].empty())
			{
				if (mask[NICK].find('.') != std::string::npos || mask[NICK].find(':') != std::string::npos)
				{
					mask[NICK].swap(mask[HOST]);
				}
			}

			for(int i = 0; i < 3; i++)
			{
				if(mask[i].empty())
				{
					mask[i].assign("*");
				}
			}

			param.assign(mask[NICK]).append(1, '!').append(mask[IDENT]).append(1, '@').append(mask[HOST]);

			if(mask[CHAN].length())
			{
				if (adding && IS_LOCAL(source))
				{
					if (!ServerInstance->IsChannel(mask[CHAN].c_str(),  ServerInstance->Config->Limits.ChanMax))
					{
						source->WriteNumeric(403, "%s %s :Invalid channel name in redirection (%s)", source->nick.c_str(), channel->name.c_str(), mask[CHAN].c_str());
						return false;
					}

					Channel *c = ServerInstance->FindChan(mask[CHAN]);
					if (!c)
					{
						source->WriteNumeric(690, "%s :Target channel %s must exist to be set as a redirect.",source->nick.c_str(),mask[CHAN].c_str());
						return false;
					}
					else if (adding && c->GetPrefixValue(source) < OP_VALUE)
					{
						source->WriteNumeric(690, "%s :You must be opped on %s to set it as a redirect.",source->nick.c_str(), mask[CHAN].c_str());
						return false;
					}

					if (assign(channel->name) == mask[CHAN])
					{
						source->WriteNumeric(690, "%s %s :You cannot set a ban redirection to the channel the ban is on", source->nick.c_str(), channel->name.c_str());
						return false;
					}
				}

				if(adding)
				{
					/* It's a properly valid redirecting ban, and we're adding it */
					redirects = extItem.get(channel);
					if (!redirects)
					{
						redirects = new BanRedirectList;
						extItem.set(channel, redirects);
					}
					else
					{
						for (BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); ++redir)
						{
							// Mimic the functionality used when removing the mode
							if ((irc::string(redir->targetchan.c_str()) == irc::string(mask[CHAN].c_str())) && (irc::string(redir->banmask.c_str()) == irc::string(param.c_str())))
							{
								// Make sure the +b handler will still set the right ban
								param.append(mask[CHAN]);
								// Silently ignore the duplicate and don't set metadata
								// This still allows channel ops to set/unset a redirect ban to clear "ghost" redirects
								return true;
							}
						}
					}

					/* Here 'param' doesn't have the channel on it yet */
					redirects->push_back(BanRedirectEntry(mask[CHAN], param));

					/* Now it does */
					param.append(mask[CHAN]);
				}
				else
				{
					/* Removing a ban, if there's no extensible there are no redirecting bans and we're fine. */
					redirects = extItem.get(channel);
					if (redirects)
					{
						/* But there were, so we need to remove the matching one if there is one */

						for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
						{
							/* Ugly as fuck */
							if((irc::string(redir->targetchan.c_str()) == irc::string(mask[CHAN].c_str())) && (irc::string(redir->banmask.c_str()) == irc::string(param.c_str())))
							{
								redirects->erase(redir);

								if(redirects->empty())
								{
									extItem.unset(channel);
								}

								break;
							}
						}
					}

					/* Append the channel so the default +b handler can remove the entry too */
					param.append(mask[CHAN]);
				}
			}
		}

		return true;
	}
};

class ModuleBanRedirect : public Module
{
	BanRedirect re;
	bool nofollow;

 public:
	ModuleBanRedirect()
	: re(this)
	{
		nofollow = false;
	}


	void init()
	{
		if(!ServerInstance->Modes->AddModeWatcher(&re))
			throw ModuleException("Could not add mode watcher");

		ServerInstance->Modules->AddService(re.extItem);
		Implementation list[] = { I_OnUserPreJoin };
		ServerInstance->Modules->Attach(list, this, sizeof(list)/sizeof(Implementation));
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_CHANNEL)
		{
			Channel* chan = static_cast<Channel*>(item);
			BanRedirectList* redirects = re.extItem.get(chan);

			if(redirects)
			{
				irc::modestacker modestack(false);
				StringDeque stackresult;
				std::vector<std::string> mode_junk;
				mode_junk.push_back(chan->name);

				for(BanRedirectList::iterator i = redirects->begin(); i != redirects->end(); i++)
				{
					modestack.Push('b', i->targetchan.insert(0, i->banmask));
				}

				for(BanRedirectList::iterator i = redirects->begin(); i != redirects->end(); i++)
				{
					modestack.PushPlus();
					modestack.Push('b', i->banmask);
				}

				while(modestack.GetStackedLine(stackresult))
				{
					mode_junk.insert(mode_junk.end(), stackresult.begin(), stackresult.end());
					ServerInstance->SendMode(mode_junk, ServerInstance->FakeClient);
					mode_junk.erase(mode_junk.begin() + 1, mode_junk.end());
				}
			}
		}
	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			BanRedirectList* redirects = re.extItem.get(chan);

			if (redirects)
			{
				/* We actually had some ban redirects to check */

				/* This was replaced with user->MakeHostIP() when I had a snprintf(), but MakeHostIP() doesn't seem to add the nick.
				 * Maybe we should have a GetFullIPHost() or something to match GetFullHost() and GetFullRealHost?
				 */

				ModResult result;
				FIRST_MOD_RESULT(OnCheckChannelBan, result, (user, chan));
				if (result == MOD_RES_ALLOW)
				{
					// they have a ban exception
					return MOD_RES_PASSTHRU;
				}

				std::string ipmask(user->nick);
				ipmask.append(1, '!').append(user->MakeHostIP());

				for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
				{
					if(InspIRCd::Match(user->GetFullRealHost(), redir->banmask) || InspIRCd::Match(user->GetFullHost(), redir->banmask) || InspIRCd::MatchCIDR(ipmask, redir->banmask))
					{
						/* This prevents recursion when a user sets multiple ban redirects in a chain
						 * (thanks Potter)
						 *
						 * If we're here and nofollow is true then we're already redirecting this user
						 * and there's a redirecting ban set on this channel that matches him, too.
						 * Deny both joins.
						 */
						if (nofollow)
							return MOD_RES_DENY;

						/* tell them they're banned and are being transferred */
						Channel* destchan = ServerInstance->FindChan(redir->targetchan);
						std::string destlimit;

						if (destchan)
							destlimit = destchan->GetModeParameter('l');

						if(destchan && ServerInstance->Modules->Find("m_redirect.so") && destchan->IsModeSet('L') && !destlimit.empty() && (destchan->GetUserCounter() >= atoi(destlimit.c_str())))
						{
							user->WriteNumeric(474, "%s %s :Cannot join channel (You are banned)", user->nick.c_str(), chan->name.c_str());
							return MOD_RES_DENY;
						}
						else
						{
							user->WriteNumeric(474, "%s %s :Cannot join channel (You are banned)", user->nick.c_str(), chan->name.c_str());
							user->WriteNumeric(470, "%s %s %s :You are banned from this channel, so you are automatically transferred to the redirected channel.", user->nick.c_str(), chan->name.c_str(), redir->targetchan.c_str());
							nofollow = true;
							Channel::JoinUser(user, redir->targetchan.c_str(), false, "", false, ServerInstance->Time());
							nofollow = false;
							return MOD_RES_DENY;
						}
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleBanRedirect()
	{
		/* XXX is this the best place to do this? */
		if (!ServerInstance->Modes->DelModeWatcher(&re))
			ServerInstance->Logs->Log("m_banredirect.so", DEBUG, "Failed to delete modewatcher!");
	}

	virtual Version GetVersion()
	{
		return Version("Allows an extended ban (+b) syntax redirecting banned users to another channel", VF_COMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleBanRedirect)
