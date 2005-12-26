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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"
#include "helperfuncs.h"

/* $ModDesc: Implements config tags which allow blocking of joins to channels */

class ModuleDenyChannels : public Module
{
 private:

	Server *Srv;
	ConfigReader *Conf;

 public:
	ModuleDenyChannels(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Conf = new ConfigReader;
	}
	
	virtual ~ModuleDenyChannels()
	{
		delete Conf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = 1;
	}

        virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
        {
		for (int j =0; j < Conf->Enumerate("badchan"); j++)
		{
			irc::string cn = Conf->ReadValue("badchan","name",j).c_str();
			irc::string thischan = cname;
			if (thischan == cn)
			{
				if ((Conf->ReadFlag("badchan","allowopers",j)) && (strchr(user->modes,'o')))
				{
					return 0;
				}
				else
				{
					std::string reason = Conf->ReadValue("badchan","reason",j);
					WriteServ(user->fd,"926 %s %s :Channel %s is forbidden: %s",user->nick,cname,cname,reason.c_str());
					return 1;
				}
			}
		}
		return 0;
        }
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleDenyChannelsFactory : public ModuleFactory
{
 public:
	ModuleDenyChannelsFactory()
	{
	}
	
	~ModuleDenyChannelsFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleDenyChannels(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleDenyChannelsFactory;
}

