/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include <map>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "configreader.h"
#include "inspircd.h"

/* $ModDesc: Provides channel mode +j (join flood protection) */

extern InspIRCd* ServerInstance;

class joinfloodsettings : public classbase
{
 public:

	int secs;
	int joins;
	time_t reset;
	time_t unlocktime;
	int counter;
	bool locked;

	joinfloodsettings() : secs(0), joins(0) {};
	joinfloodsettings(int b, int c) : secs(b), joins(c)
	{
		reset = time(NULL) + secs;
		counter = 0;
		locked = false;
		log(DEBUG,"Create new joinfloodsettings: %lu %lu",time(NULL),reset);
	};

	void addjoin()
	{
		counter++;
		log(DEBUG,"joinflood counter is %d",counter);
		if (time(NULL) > reset)
		{
			log(DEBUG,"joinflood counter reset");
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
		log(DEBUG,"joinflood counter clear");
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
		log(DEBUG,"joinflood lock");
		locked = true;
		unlocktime = time(NULL) + 60;
	}

};

class JoinFlood : public ModeHandler
{
 public:
	JoinFlood() : ModeHandler('j', 1, 0, false, MODETYPE_CHANNEL, false) { }

        ModePair ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
        {
                joinfloodsettings* x;
                if (channel->GetExt("joinflood",x))
                        return std::make_pair(true, ConvToStr(x->joins)+":"+ConvToStr(x->secs));
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
					parameter = "";
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
				DELETE(f);
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
	Server *Srv;
	JoinFlood* jf;
	
 public:
 
	ModuleJoinFlood(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		jf = new JoinFlood();
		Srv->AddMode(jf, 'j');
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
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

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		joinfloodsettings *f;
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

	void OnChannelDelete(chanrec* chan)
	{
		joinfloodsettings *f;
		if (chan->GetExt("joinflood",f))
		{
			DELETE(f);
			chan->Shrink("joinflood");
		}
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnChannelDelete] = List[I_OnUserPreJoin] = List[I_OnUserJoin] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->ModeGrok->InsertMode(output, "j", 3);
	}

	virtual ~ModuleJoinFlood()
	{
		DELETE(jf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleJoinFloodFactory : public ModuleFactory
{
 public:
	ModuleJoinFloodFactory()
	{
	}
	
	~ModuleJoinFloodFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleJoinFlood(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleJoinFloodFactory;
}

