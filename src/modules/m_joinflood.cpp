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

/* $ModDesc: Provides channel mode +j (join flood protection) */

/** Holds settings and state associated with channel mode +j
 */
class joinfloodsettings : public classbase
{
 private:
	InspIRCd* ServerInstance;
 public:
	int secs;
	int joins;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;

	joinfloodsettings(InspIRCd *Instance, int b, int c) : ServerInstance(Instance), secs(b), joins(c)
	{
		reset = ServerInstance->Time() + secs;
		counter = 0;
		locked = false;
	};

	void addjoin()
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
		return (counter >= this->joins);
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
class JoinFlood : public ModeHandler
{
 public:
	JoinFlood(InspIRCd* Instance, Module* Creator) : ModeHandler(Instance, Creator, 'j', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		joinfloodsettings* x;
		if (channel->GetExt("joinflood",x))
			return std::make_pair(true, ConvToStr(x->joins)+":"+ConvToStr(x->secs));
		else
			return std::make_pair(false, parameter);
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		joinfloodsettings* dummy;

		if (adding)
		{
			char ndata[MAXBUF];
			char* data = ndata;
			strlcpy(ndata,parameter.c_str(),MAXBUF);
			char* joins = data;
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
				int njoins = atoi(joins);
				int nsecs = atoi(secs);
				if ((njoins<1) || (nsecs<1))
				{
					source->WriteNumeric(608, "%s %s :Invalid flood parameter",source->nick.c_str(),channel->name.c_str());
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					if (!channel->GetExt("joinflood", dummy))
					{
						parameter = ConvToStr(njoins) + ":" +ConvToStr(nsecs);
						joinfloodsettings *f = new joinfloodsettings(ServerInstance, nsecs, njoins);
						channel->Extend("joinflood", f);
						channel->SetModeParam('j', parameter);
						return MODEACTION_ALLOW;
					}
					else
					{
						std::string cur_param = channel->GetModeParameter('j');
						parameter = ConvToStr(njoins) + ":" +ConvToStr(nsecs);
						if (cur_param == parameter)
						{
							// mode params match
							return MODEACTION_DENY;
						}
						else
						{
							// new mode param, replace old with new
							if ((nsecs > 0) && (njoins > 0))
							{
								joinfloodsettings* f;
								channel->GetExt("joinflood", f);
								delete f;
								f = new joinfloodsettings(ServerInstance, nsecs, njoins);
								channel->Shrink("joinflood");
								channel->Extend("joinflood", f);
								channel->SetModeParam('j', parameter);
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
			if (channel->GetExt("joinflood", dummy))
			{
				joinfloodsettings *f;
				channel->GetExt("joinflood", f);
				delete f;
				channel->Shrink("joinflood");
				channel->SetModeParam('j', "");
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleJoinFlood : public Module
{

	JoinFlood jf;

 public:

	ModuleJoinFlood(InspIRCd* Me)
		: Module(Me), jf(Me, this)
	{

		if (!ServerInstance->Modes->AddMode(&jf))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnChannelDelete, I_OnUserPreJoin, I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs, const std::string &keygiven)
	{
		if (chan)
		{
			joinfloodsettings *f;
			if (chan->GetExt("joinflood", f))
			{
				if (f->islocked())
				{
					user->WriteNumeric(609, "%s %s :This channel is temporarily unavailable (+j). Please try again later.",user->nick.c_str(),chan->name.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent, bool created)
	{
		joinfloodsettings *f;

		/* We arent interested in JOIN events caused by a network burst */
		if (sync)
			return;

		/* But all others are OK */
		if (channel->GetExt("joinflood",f))
		{
			f->addjoin();
			if (f->shouldlock())
			{
				f->clear();
				f->lock();
				channel->WriteChannelWithServ((char*)ServerInstance->Config->ServerName, "NOTICE %s :This channel has been closed to new users for 60 seconds because there have been more than %d joins in %d seconds.", channel->name.c_str(), f->joins, f->secs);
			}
		}
	}

	void OnChannelDelete(Channel* chan)
	{
		joinfloodsettings *f;
		if (chan->GetExt("joinflood",f))
		{
			delete f;
			chan->Shrink("joinflood");
		}
	}


	virtual ~ModuleJoinFlood()
	{
		ServerInstance->Modes->DelMode(&jf);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleJoinFlood)
