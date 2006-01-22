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

/* $ModDesc: Provides channel mode +f (message flood protection) */

class floodsettings
{
 public:
	bool ban;
	int secs;
	int lines;
	time_t reset;
	std::map<userrec*,int> counters;

	floodsettings() : ban(0), secs(0), lines(0) {};
	floodsettings(bool a, int b, int c) : ban(a), secs(b), lines(c)
	{
		reset = time(NULL) + secs;
		log(DEBUG,"Create new floodsettings: %lu %lu",time(NULL),reset);
	};

	void addmessage(userrec* who)
	{
		std::map<userrec*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			iter->second++;
			log(DEBUG,"Count for %s is now %d",who->nick,iter->second);
		}
		else
		{
			counters[who] = 1;
			log(DEBUG,"Count for %s is now *1*",who->nick);
		}
		if (time(NULL) > reset)
		{
			log(DEBUG,"floodsettings timer Resetting.");
			counters.clear();
			reset = time(NULL) + secs;
		}
	}

	bool shouldkick(userrec* who)
	{
		std::map<userrec*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			return (iter->second >= this->lines);
		}
		else return false;
	}

	void clear(userrec* who)
	{
		std::map<userrec*,int>::iterator iter = counters.find(who);
		if (iter != counters.end())
		{
			counters.erase(iter);
		}
	}
};

class ModuleMsgFlood : public Module
{
	Server *Srv;
	
 public:
 
	ModuleMsgFlood(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('f',MT_CHANNEL,false,1,0);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'f') && (type == MT_CHANNEL))
		{
			if (mode_on)
			{
				std::string FloodParams = params[0];
				chanrec* c = (chanrec*)target;
				char ndata[MAXBUF];
				char* data = ndata;
				strlcpy(ndata,FloodParams.c_str(),MAXBUF);
				char* lines = data;
				char* secs = NULL;
				bool ban = false;
				if (*data == '*')
				{
					ban = true;
					lines++;
				}
				else
				{
					ban = false;
				}
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
					int nlines = atoi(lines);
					int nsecs = atoi(secs);
					if ((nlines<1) || (nsecs<1))
					{
						WriteServ(user->fd,"608 %s %s :Invalid flood parameter",user->nick,c->name);
						return 0;
					}
					else
					{
						if (!c->GetExt("flood"))
						{
							floodsettings *f = new floodsettings(ban,nlines,nsecs);
							c->Extend("flood",(char*)f);
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
				if (c->GetExt("flood"))
				{
					floodsettings *f = (floodsettings*)c->GetExt("flood");
					delete f;
					c->Shrink("flood");
				}
			}
			return 1;
		}
		return 0;
	}

	int ProcessMessages(userrec* user,chanrec* dest,std::string &text)
	{
		floodsettings *f = (floodsettings*)dest->GetExt("flood");
		if (f)
		{
			f->addmessage(user);
			if (f->shouldkick(user))
			{
				/* Youre outttta here! */
				f->clear(user);
				Srv->KickUser(NULL, user, dest, "Channel flood triggered (mode +f)");
				return 1;
			}
		}
		return 0;
	}

        virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text)
        {
                if (target_type == TYPE_CHANNEL)
                {
                        return ProcessMessages(user,(chanrec*)dest,text);
                }
                else return 0;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			return ProcessMessages(user,(chanrec*)dest,text);
		}
		else return 0;
	}

	void OnChannelDelete(chanrec* chan)
	{
		if (chan->GetExt("flood"))
		{
			floodsettings *f = (floodsettings*)chan->GetExt("flood");
			delete f;
			chan->Shrink("flood");
		}
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnExtendedMode] = List[I_OnChannelDelete] = List[I_OnUserPreNotice] = List[I_OnUserPreMessage] = 1;
	}

        virtual void On005Numeric(std::string &output)
        {
                std::stringstream line(output);
                std::string temp1, temp2;
                while (!line.eof())
                {
                        line >> temp1;
                        if (temp1.substr(0,10) == "CHANMODES=")
                        {
				// By doing this we're *assuming* no other module has fucked up the CHANMODES=
				// section of the 005 numeric. If they have, we're going DOWN in a blaze of glory,
				// with a honking great EXCEPTION :)
				temp1.insert(temp1.find(",")+1,"f");
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }

	virtual ~ModuleMsgFlood()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleMsgFloodFactory : public ModuleFactory
{
 public:
	ModuleMsgFloodFactory()
	{
	}
	
	~ModuleMsgFloodFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleMsgFlood(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleMsgFloodFactory;
}

