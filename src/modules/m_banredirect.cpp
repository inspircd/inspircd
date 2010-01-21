/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
/* $ModDep: ../../include/u_listmode.h */

/* Originally written by Om, January 2009
 */

class BanRedirectEntry : public classbase
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
	BanRedirect(InspIRCd* Instance)
	: ModeWatcher(Instance, 'b', MODETYPE_CHANNEL)
	{
	}

	bool BeforeMode(User* source, User* dest, Channel* channel, std::string &param, bool adding, ModeType type, bool)
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

			if(adding && (channel->bans.size() > static_cast<unsigned>(maxbans)))
			{
				source->WriteNumeric(478, "%s %s :Channel ban list for %s is full (maximum entries for this channel is %ld)", source->nick.c_str(), channel->name.c_str(), channel->name.c_str(), maxbans);
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
					else if (c->GetStatus(source) < STATUS_OP)
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
					if(!channel->GetExt("banredirects", redirects))
					{
						redirects = new BanRedirectList;
						channel->Extend("banredirects", redirects);
					}

					/* Here 'param' doesn't have the channel on it yet */
					redirects->push_back(BanRedirectEntry(mask[CHAN].c_str(), param.c_str()));

					/* Now it does */
					param.append(mask[CHAN]);
				}
				else
				{
					/* Removing a ban, if there's no extensible there are no redirecting bans and we're fine. */
					if(channel->GetExt("banredirects", redirects))
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
									delete redirects;
									channel->Shrink("banredirects");
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
	BanRedirect* re;
	bool nofollow;
	Module* ExceptionModule;

 public:
	ModuleBanRedirect(InspIRCd* Me)
	: Module(Me)
	{
		re = new BanRedirect(Me);
		nofollow = false;

		if(!ServerInstance->Modes->AddModeWatcher(re))
		{
			delete re;
			throw ModuleException("Could not add mode watcher");
		}

		OnRehash(NULL);

		Implementation list[] = { I_OnRehash, I_OnUserPreJoin, I_OnChannelDelete, I_OnCleanup };
		Me->Modules->Attach(list, this, 4);

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
			BanRedirectList* redirects;

			if(chan->GetExt("banredirects", redirects))
			{
				irc::modestacker modestack(ServerInstance, false);
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

				delete redirects;
				chan->Shrink("banredirects");
			}
		}
	}

	virtual void OnRehash(User* user)
	{
		ExceptionModule = ServerInstance->Modules->Find("m_banexception.so");
	}

	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		/* This prevents recursion when a user sets multiple ban redirects in a chain
		 * (thanks Potter)
		 */
		if (nofollow)
			return 0;

		/* Return 1 to prevent the join, 0 to allow it */
		if (chan)
		{
			BanRedirectList* redirects;

			if(chan->GetExt("banredirects", redirects))
			{
				/* We actually had some ban redirects to check */

				/* This was replaced with user->MakeHostIP() when I had a snprintf(), but MakeHostIP() doesn't seem to add the nick.
				 * Maybe we should have a GetFullIPHost() or something to match GetFullHost() and GetFullRealHost?
				 */

				if (ExceptionModule)
				{
					ListModeRequest n(this, ExceptionModule, user, chan);
					/* Users with ban exceptions are allowed to join without being redirected */
					if (n.Send())
						return 0;
				}

				std::string ipmask(user->nick);
				ipmask.append(1, '!').append(user->MakeHostIP());

				for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
				{
					if(InspIRCd::Match(user->GetFullRealHost(), redir->banmask) || InspIRCd::Match(user->GetFullHost(), redir->banmask) || InspIRCd::MatchCIDR(ipmask, redir->banmask))
					{
						/* tell them they're banned and are being transferred */
						Channel* destchan = ServerInstance->FindChan(redir->targetchan);
						std::string destlimit;

						if (destchan)
							destlimit = destchan->GetModeParameter('l');

						if(destchan && ServerInstance->Modules->Find("m_redirect.so") && destchan->IsModeSet('L') && !destlimit.empty() && (destchan->GetUserCounter() >= atoi(destlimit.c_str())))
						{
							user->WriteNumeric(474, "%s %s :Cannot join channel (You are banned)", user->nick.c_str(), chan->name.c_str());
							return 1;
						}
						else
						{
							user->WriteNumeric(474, "%s %s :Cannot join channel (You are banned)", user->nick.c_str(), chan->name.c_str());
							user->WriteNumeric(470, "%s %s %s :You are banned from this channel, so you are automatically transfered to the redirected channel.", user->nick.c_str(), chan->name.c_str(), redir->targetchan.c_str());
							nofollow = true;
							Channel::JoinUser(ServerInstance, user, redir->targetchan.c_str(), false, "", false, ServerInstance->Time());
							nofollow = false;
							return 1;
						}
					}
				}
			}
		}
		return 0;
	}

	virtual ~ModuleBanRedirect()
	{
		ServerInstance->Modes->DelModeWatcher(re);
		delete re;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON|VF_VENDOR, API_VERSION);
	}

	void Prioritize()
	{
		Module* banex = ServerInstance->Modules->Find("m_banexception.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_BEFORE, &banex);
	}
};

MODULE_INIT(ModuleBanRedirect)
