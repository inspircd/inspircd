/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
 public:

	int secs;
	int joins;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;
	InspIRCd* ServerInstance;

	joinfloodsettings() : secs(0), joins(0) {};

	joinfloodsettings(int b, int c) : secs(b), joins(c)
	{
		reset = time(NULL) + secs;
		counter = 0;
		locked = false;
	};

	void addjoin()
	{
		counter++;
		if (time(NULL) > reset)
		{
			counter = 0;
			reset = time(NULL) + secs;
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
			if (time(NULL) > unlocktime)
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
		unlocktime = time(NULL) + 60;
	}

};

/** Handles channel mode +j
 */
class JoinFlood : public ModeHandler
{
 public:
	JoinFlood(InspIRCd* Instance) : ModeHandler(Instance, 'j', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter)
	{
		joinfloodsettings* x;
		if (channel->GetExt("joinflood",x))
			return std::make_pair(true, ConvToStr(x->joins)+":"+ConvToStr(x->secs));
		else
			return std::make_pair(false, parameter);
	} 

	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, Channel* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
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
					source->WriteServ("608 %s %s :Invalid flood parameter",source->nick,channel->name);
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					if (!channel->GetExt("joinflood", dummy))
					{
						parameter = ConvToStr(njoins) + ":" +ConvToStr(nsecs);
						joinfloodsettings *f = new joinfloodsettings(nsecs,njoins);
						channel->Extend("joinflood", f);
						channel->SetMode('j', true);
						channel->SetModeParam('j', parameter.c_str(), true);
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
								f = new joinfloodsettings(nsecs,njoins);
								channel->Shrink("joinflood");
								channel->Extend("joinflood", f);
								channel->SetModeParam('j', cur_param.c_str(), false);
								channel->SetModeParam('j', parameter.c_str(), true);
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
				source->WriteServ("608 %s %s :Invalid flood parameter",source->nick,channel->name);
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
				channel->SetMode('j', false);
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleJoinFlood : public Module
{
	
	JoinFlood* jf;
	
 public:
 
	ModuleJoinFlood(InspIRCd* Me)
		: Module(Me)
	{
		
		jf = new JoinFlood(ServerInstance);
		if (!ServerInstance->AddMode(jf))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnChannelDelete, I_OnUserPreJoin, I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}
	
	virtual int OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string &privs)
	{
		if (chan)
		{
			joinfloodsettings *f;
			if (chan->GetExt("joinflood", f))
			{
				if (f->islocked())
				{
					user->WriteServ("609 %s %s :This channel is temporarily unavailable (+j). Please try again later.",user->nick,chan->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual void OnUserJoin(User* user, Channel* channel, bool sync, bool &silent)
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
				channel->WriteChannelWithServ((char*)ServerInstance->Config->ServerName, "NOTICE %s :This channel has been closed to new users for 60 seconds because there have been more than %d joins in %d seconds.", channel->name, f->joins, f->secs);
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
		ServerInstance->Modes->DelMode(jf);
		delete jf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleJoinFlood)
