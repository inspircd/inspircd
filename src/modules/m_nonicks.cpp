/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Provides support for unreal-style GLOBOPS and umode +g */

class ModuleNoNickChange : public Module
{
	Server *Srv;
	
 public:
	ModuleNoNickChange(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		
		Srv->AddExtendedMode('N',MT_CHANNEL,false,0,0);
	}
	
	virtual ~ModuleNoNickChange()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
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
                                // append the chanmode to the end
                                temp1 = temp1.substr(10,temp1.length());
                                temp1 = "CHANMODES=" + temp1 + "N";
                        }
                        temp2 = temp2 + temp1 + " ";
                }
		if (temp2.length())
	                output = temp2.substr(0,temp2.length()-1);
        }
	
	virtual int OnUserPreNick(userrec* user, std::string newnick)
	{
		irc::string server = user->server;
		irc::string me = Srv->GetServerName().c_str();
		if (server == me)
		{
			for (int i =0; i != MAXCHANS; i++)
			{
				if (user->chans[i].channel != NULL)
				{
					chanrec* curr = user->chans[i].channel;
					if (curr->IsCustomModeSet('N'))
					{
						if (!strchr(user->modes,'o'))
						{
							// don't allow the nickchange, theyre on at least one channel with +N set
							// and theyre not an oper
							WriteServ(user->fd,"447 %s :Can't change nickname while on %s (+N is set)",user->nick,curr->name);
							return 1;
						}
					}
				}
			}
		}
		return 0;
	}
 	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'N') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleNoNickChangeFactory : public ModuleFactory
{
 public:
	ModuleNoNickChangeFactory()
	{
	}
	
	~ModuleNoNickChangeFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleNoNickChange(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoNickChangeFactory;
}

