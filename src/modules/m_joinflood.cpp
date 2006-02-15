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

/* $ModDesc: Provides channel mode +j (join flood protection) */

class joinfloodsettings
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

class ModuleJoinFlood : public Module
{
	Server *Srv;
	
 public:
 
	ModuleJoinFlood(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('j',MT_CHANNEL,false,1,0);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'j') && (type == MT_CHANNEL))
		{
			if (mode_on)
			{
				std::string FloodParams = params[0];
				chanrec* c = (chanrec*)target;
				char ndata[MAXBUF];
				char* data = ndata;
				strlcpy(ndata,FloodParams.c_str(),MAXBUF);
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
						WriteServ(user->fd,"608 %s %s :Invalid flood parameter",user->nick,c->name);
						return 0;
					}
					else
					{
						if (!c->GetExt("joinflood"))
						{
							joinfloodsettings *f = new joinfloodsettings(nsecs,njoins);
							c->Extend("joinflood",(char*)f);
						}
					}
					return 1;
				}
				else
				{
					WriteServ(user->fd,"608 %s %s :Invalid flood parameter",user->nick,c->name);
					return 0;
				}
				
			}
			else
			{
				chanrec* c = (chanrec*)target;
				if (c->GetExt("joinflood"))
				{
					joinfloodsettings *f = (joinfloodsettings*)c->GetExt("joinflood");
					delete f;
					c->Shrink("joinflood");
				}
			}
			return 1;
		}
		return 0;
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (chan)
		{
			joinfloodsettings *f = (joinfloodsettings*)chan->GetExt("joinflood");
			if (f)
			{
				if (f->islocked())
				{
					WriteServ(user->fd,"609 %s %s :This channel is temporarily unavailable (+j). Please try again later.",user->nick,chan->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		joinfloodsettings *f = (joinfloodsettings*)channel->GetExt("joinflood");
		if (f)
		{
			f->addjoin();
			if (f->shouldlock())
			{
				f->clear();
				f->lock();
				WriteChannelWithServ((char*)Srv->GetServerName().c_str(), channel, "NOTICE %s :This channel has been closed to new users for 60 seconds because there have been more than %d joins in %d seconds.",channel->name,f->joins,f->secs);
			}
		}
	}

	void OnChannelDelete(chanrec* chan)
	{
		if (chan->GetExt("joinflood"))
		{
			joinfloodsettings *f = (joinfloodsettings*)chan->GetExt("joinflood");
			delete f;
			chan->Shrink("joinflood");
		}
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnExtendedMode] = List[I_OnChannelDelete] = List[I_OnUserPreJoin] = List[I_OnUserJoin] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "j", 3);
	}

	virtual ~ModuleJoinFlood()
	{
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

