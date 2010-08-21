/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

			if (param.length() >= 2 && param[1] == ':')
				return true;

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

					Channel *c = ServerInstance->FindChan(mask[CHAN].c_str());
					if (!c)
					{
						source->WriteNumeric(690, "%s :Target channel %s must exist to be set as a redirect.",source->nick.c_str(),mask[CHAN].c_str());
						return false;
					}
					else if (adding && c->GetAccessRank(source) < OP_VALUE)
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

					/* Here 'param' doesn't have the channel on it yet */
					redirects->push_back(BanRedirectEntry(mask[CHAN].c_str(), param.c_str()));

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

		ServerInstance->Extensions.Register(&re.extItem);
		Implementation list[] = { I_OnCheckJoin, I_OnChannelDelete };
		ServerInstance->Modules->Attach(list, this, 2);
	}

	virtual void OnChannelDelete(Channel* chan)
	{
		OnCleanup(TYPE_CHANNEL, chan);
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_CHANNEL)
		{
			Channel* chan = static_cast<Channel*>(item);
			BanRedirectList* redirects = re.extItem.get(chan);

			if(redirects)
			{
				irc::modestacker modestack;
				StringDeque stackresult;
				std::vector<std::string> mode_junk;
				mode_junk.push_back(chan->name);

				for(BanRedirectList::iterator i = redirects->begin(); i != redirects->end(); i++)
				{
					modestack.push(irc::modechange("ban", i->targetchan.insert(0, i->banmask), false));
				}

				for(BanRedirectList::iterator i = redirects->begin(); i != redirects->end(); i++)
				{
					modestack.push(irc::modechange("ban", i->banmask, true));
				}

				ServerInstance->SendMode(ServerInstance->FakeClient, chan, modestack, false);
			}
		}
	}

	virtual void OnCheckJoin(ChannelPermissionData& join)
	{
		if (!join.chan || join.result != MOD_RES_PASSTHRU)
			return;

		BanRedirectList* redirects = re.extItem.get(join.chan);
		if (!redirects)
			return;
		/* We actually had some ban redirects to check */

		/* First, allow all normal +e exemptions to work */
		ModResult result;
		FIRST_MOD_RESULT(OnCheckChannelBan, result, (join.user, join.chan));
		if (result != MOD_RES_PASSTHRU)
			return;

		for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
		{
			if (join.chan->CheckBan(join.user, redir->banmask))
			{
				join.result = MOD_RES_DENY;
				/* tell them they're banned and are being transferred */
				join.user->WriteNumeric(474, "%s %s :Cannot join channel (You are banned)",
					join.user->nick.c_str(), join.chan->name.c_str());
				
				// double redirects are not allowed
				if (ServerInstance->RedirectJoin.get(join.user))
					return;
				
				join.user->WriteNumeric(470, "%s %s %s :You are banned from this channel, so you are automatically transfered to the redirected channel.",
					join.user->nick.c_str(), join.chan->name.c_str(), redir->targetchan.c_str());
				ServerInstance->RedirectJoin.set(join.user, 1);
				Channel::JoinUser(join.user, redir->targetchan.c_str(), false, "", false, ServerInstance->Time());
				ServerInstance->RedirectJoin.set(join.user, 0);
				return;
			}
		}
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

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnCheckJoin, PRIORITY_LAST);
	}
};

MODULE_INIT(ModuleBanRedirect)
