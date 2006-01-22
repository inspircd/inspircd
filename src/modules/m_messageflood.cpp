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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides channel mode +f (message flood protection) */

class floodsettings
{
	bool ban;
	int secs;
	int lines;

	floodsettings() : ban(0), secs(0), lines(0) {};
	floodsettings(bool a, int b, int c) : ban(a), secs(b), lines(c) {};
}

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
				char data[MAXBUF];
				strlcpy(data,FloodParams.c_str(),MAXBUF);
				char* lines = data;
				char* secs = NULL;
				bool ban = false;
				if (*data == '*')
				{
					ban = true;
					data++;
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

	void OnChannelDelete(chanrec* chan)
	{
		if (c->GetExt("flood"))
		{
			floodsettings *f = (floodsettings*)c->GetExt("flood");
			delete f;
			c->Shrink("flood");
		}
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnExtendedMode] = List[I_OnChannelDelete] = 1;
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
				temp1.insert(temp1.find(",")+1,"L");
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

