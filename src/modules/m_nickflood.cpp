/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides channel mode +F (nick flood protection) */

/** Holds settings and state associated with channel mode +F
 */
class nickfloodsettings : public classbase
{
 private:
	InspIRCd* ServerInstance;
 public:
	int secs;
	int nicks;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;

	nickfloodsettings(InspIRCd *Instance, int b, int c) : ServerInstance(Instance), secs(b), nicks(c)
	{
		reset = Instance->Time() + secs;
		counter = 0;
		locked = false;
	};

	void addnick()
	{
		counter++;
		if (ServerInstance->Time() > reset)
		{
			counter = 0;
			reset = ServerInstance->Time() + secs;
		}
	}

	bool shouldlock()
	{
		/* XXX HACK: using counter + 1 here now to allow the counter to only be incremented
		 * on successful nick changes; this will be checked before the counter is
		 * incremented.
		 */
		return (counter + 1 >= this->nicks);
	}

	void clear()
	{
		counter = 0;
	}

	bool islocked()
	{
		if (locked)
		{
			if (ServerInstance->Time() > unlocktime)
			{
				locked = false;
				return false;
			}
			else
			{
				return true;
			}
		}
		return false;
	}

	void lock()
	{
		locked = true;
		unlocktime = ServerInstance->Time() + 60;
	}

};

/** Handles channel mode +j
 */
class NickFlood : public ModeHandler
{
 public:
	NickFlood(InspIRCd* Instance) : ModeHandler(Instance, 'F', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		nickfloodsettings* x;
		if (channel->GetExt("nickflood",x))
			return std::make_pair(true, ConvToStr(x->nicks)+":"+ConvToStr(x->secs));
		else
			return std::make_pair(false, parameter);
	}

	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, Channel* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		nickfloodsettings* dummy;

		if (adding)
		{
			char ndata[MAXBUF];
			char* data = ndata;
			strlcpy(ndata,parameter.c_str(),MAXBUF);
			char* nicks = data;
			char* secs = NULL;
			while (*data)
			{
				if (*data == ':')
				{
					*data = 0;
					data++;
					secs = data;
					break;
				}
				else data++;
			}
			if (secs)

			{
				/* Set up the flood parameters for this channel */
				int nnicks = atoi(nicks);
				int nsecs = atoi(secs);
				if ((nnicks<1) || (nsecs<1))
				{
					source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					if (!channel->GetExt("nickflood", dummy))
					{
						parameter = ConvToStr(nnicks) + ":" +ConvToStr(nsecs);
						nickfloodsettings *f = new nickfloodsettings(ServerInstance, nsecs, nnicks);
						channel->Extend("nickflood", f);
						channel->SetModeParam('F', parameter);
						return MODEACTION_ALLOW;
					}
					else
					{
						std::string cur_param = channel->GetModeParameter('F');
						parameter = ConvToStr(nnicks) + ":" +ConvToStr(nsecs);
						if (cur_param == parameter)
						{
							// mode params match
							return MODEACTION_DENY;
						}
						else
						{
							// new mode param, replace old with new
							if ((nsecs > 0) && (nnicks > 0))
							{
								nickfloodsettings* f;
								channel->GetExt("nickflood", f);
								delete f;
								f = new nickfloodsettings(ServerInstance, nsecs, nnicks);
								channel->Shrink("nickflood");
								channel->Extend("nickflood", f);
								channel->SetModeParam('F', parameter);
								return MODEACTION_ALLOW;
							}
							else
							{
								return MODEACTION_DENY;
							}
						}
					}
				}
			}
			else
			{
				source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->GetExt("nickflood", dummy))
			{
				nickfloodsettings *f;
				channel->GetExt("nickflood", f);
				delete f;
				channel->Shrink("nickflood");
				channel->SetModeParam('F', "");
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleNickFlood : public Module
{
	NickFlood* jf;

 public:

	ModuleNickFlood(InspIRCd* Me)
		: Module(Me)
	{

		jf = new NickFlood(ServerInstance);
		if (!ServerInstance->Modes->AddMode(jf))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnChannelDelete, I_OnUserPreNick, I_OnUserPostNick };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual int OnUserPreNick(User* user, const std::string &newnick)
	{
		if (isdigit(newnick[0])) /* allow switches to UID */
			return 0;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel *channel = i->first;

			nickfloodsettings *f;
			if (channel->GetExt("nickflood", f))
			{
				if (CHANOPS_EXEMPT(ServerInstance, 'F') && channel->GetStatus(user) == STATUS_OP)
					continue;

				if (f->islocked())
				{
					user->WriteNumeric(447, "%s :%s has been locked for nickchanges for 60 seconds because there have been more than %d nick changes in %d seconds", user->nick.c_str(), channel->name.c_str(), f->nicks, f->secs);
					return 1;
				}

				if (f->shouldlock())
				{
					f->clear();
					f->lock();
					channel->WriteChannelWithServ((char*)ServerInstance->Config->ServerName, "NOTICE %s :No nick changes are allowed for 60 seconds because there have been more than %d nick changes in %d seconds.", channel->name.c_str(), f->nicks, f->secs);
					return 1;
				}
			}
		}

		return 0;
	}

	/*
	 * XXX: HACK: We do the increment on the *POST* event here (instead of all together) because we have no way of knowing whether other modules would block a nickchange.
	 */
	virtual void OnUserPostNick(User* user, const std::string &oldnick)
	{
		if (isdigit(user->nick[0])) /* allow switches to UID */
			return;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); ++i)
		{
			Channel *channel = i->first;

			nickfloodsettings *f;
			if (channel->GetExt("nickflood", f))
			{
				if (CHANOPS_EXEMPT(ServerInstance, 'F') && channel->GetStatus(user) == STATUS_OP)
					return;
				
				/* moved this here to avoid incrementing the counter for nick
				 * changes that are denied for some other reason (bans, +N, etc.)
				 * per bug #874.
				 */
				f->addnick();
			}
		}
		return;
	}

	void OnChannelDelete(Channel* chan)
	{
		nickfloodsettings *f;
		if (chan->GetExt("nickflood",f))
		{
			delete f;
			chan->Shrink("nickflood");
		}
	}


	virtual ~ModuleNickFlood()
	{
		ServerInstance->Modes->DelMode(jf);
		delete jf;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleNickFlood)
