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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides channel mode +F (nick flood protection) */

/** Holds settings and state associated with channel mode +F
 */
class nickfloodsettings : public classbase
{
 public:

	int secs;
	int nicks;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;
	InspIRCd* ServerInstance;

	nickfloodsettings() : secs(0), nicks(0) {};

	nickfloodsettings(int b, int c) : secs(b), nicks(c)
	{
		reset = time(NULL) + secs;
		counter = 0;
		locked = false;
	};

	void addnick()
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
		return (counter >= this->nicks);
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
class NickFlood : public ModeHandler
{
 public:
	NickFlood(InspIRCd* Instance) : ModeHandler(Instance, 'F', 1, 0, false, MODETYPE_CHANNEL, false) { }

	ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
	{
		nickfloodsettings* x;
		if (channel->GetExt("nickflood",x))
			return std::make_pair(true, ConvToStr(x->nicks)+":"+ConvToStr(x->secs));
		else
			return std::make_pair(false, parameter);
	} 

	bool CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel)
	{
		/* When TS is equal, the alphabetically later one wins */
		return (their_param < our_param);
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
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
					source->WriteServ("608 %s %s :Invalid flood parameter",source->nick,channel->name);
					parameter.clear();
					return MODEACTION_DENY;
				}
				else
				{
					if (!channel->GetExt("nickflood", dummy))
					{
						parameter = ConvToStr(nnicks) + ":" +ConvToStr(nsecs);
						nickfloodsettings *f = new nickfloodsettings(nsecs,nnicks);
						channel->Extend("nickflood", f);
						channel->SetMode('F', true);
						channel->SetModeParam('F', parameter.c_str(), true);
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
								f = new nickfloodsettings(nsecs, nnicks);
								channel->Shrink("nickflood");
								channel->Extend("nickflood", f);
								channel->SetModeParam('F', cur_param.c_str(), false);
								channel->SetModeParam('F', parameter.c_str(), true);
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
			if (channel->GetExt("nickflood", dummy))
			{
				nickfloodsettings *f;
				channel->GetExt("nickflood", f);
				DELETE(f);
				channel->Shrink("nickflood");
				channel->SetMode('F', false);
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
		if (!ServerInstance->AddMode(jf, 'F'))
			throw ModuleException("Could not add new modes!");
	}

	virtual int OnUserPreNick(userrec* user, const std::string &newnick)
	{
		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			chanrec *channel = i->first;

			nickfloodsettings *f;
			if (channel->GetExt("nickflood", f))
			{
				if (CHANOPS_EXEMPT(ServerInstance, 'F') && channel->GetStatus(user) == STATUS_OP)
					continue;

				if (f->islocked())
				{
					user->WriteServ("447 %s :%s has been locked for nickchanges for 60 seconds because there have been more than %d nick changes in %d seconds", user->nick, channel->name, f->nicks, f->secs);
					return 1;
				}

				f->addnick();
				if (f->shouldlock())
				{
					f->clear();
					f->lock();
					channel->WriteChannelWithServ((char*)ServerInstance->Config->ServerName, "NOTICE %s :No nick changes are allowed for 60 seconds because there have been more than %d nick changes in %d seconds.", channel->name, f->nicks, f->secs);
					return 1;
				}
			}
		}

		return 0;
	}
	
	void OnChannelDelete(chanrec* chan)
	{
		nickfloodsettings *f;
		if (chan->GetExt("nickflood",f))
		{
			DELETE(f);
			chan->Shrink("nickflood");
		}
	}

	void Implements(char* List)
	{
		List[I_OnChannelDelete] = List[I_OnUserPreNick] = 1;
	}

	virtual ~ModuleNickFlood()
	{
		ServerInstance->Modes->DelMode(jf);
		DELETE(jf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleNickFlood)
