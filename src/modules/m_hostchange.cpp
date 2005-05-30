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

/* $ModDesc: Provides masking of user hostnames in a different way to m_cloaking */

class ModuleHostChange : public Module
{
 private:

	Server *Srv;
	ConfigReader *Conf;
	std::string MySuffix;
	 
 public:
	ModuleHostChange()
	{
		// We must create an instance of the Server class to work with
		Srv = new Server;
                Conf = new ConfigReader;
		MySuffix = Conf->ReadValue("host","suffix",0);
	}
	
	virtual ~ModuleHostChange()
	{
		// not really neccessary, but free it anyway
		delete Srv;
		delete Conf;
	}

	virtual void OnRehash()
	{
		delete Conf;
		Conf = new ConfigReader;
		MySuffix = Conf->ReadValue("host","suffix",0);
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,0,0,1,VF_VENDOR);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		for (int index = 0; index < Conf->Enumerate("hostchange"); index++)
		{
			std::string mask = Conf->ReadValue("hostchange","mask",index);
			if (Srv->MatchText(std::string(user->ident)+"@"+std::string(user->host),mask))
			{
				std::string newhost = "";
				// host of new user matches a hostchange tag's mask
				std::string action = Conf->ReadValue("hostchange","action",index);
				if (action == "set")
				{
					newhost = Conf->ReadValue("hostchange","value",index);
				}
				else if (action == "suffix")
				{
					newhost = MySuffix;
				}
				else if (action == "addnick")
				{
					// first take their nick and strip out non-dns, leaving just [A-Z0-9\-]
					std::string complete = "";
					std::string old = user->nick;
					for (unsigned int j = 0; j < old.length(); j++)
					{
						if  (((old[j] >= 'A') && (old[j] <= 'Z')) ||
						    ((old[j] >= 'a') && (old[j] <= 'z')) ||
						    ((old[j] >= '0') && (old[j] <= '9')) ||
						    (old[j] == '-'))
						{
							complete = complete + old[j];
						}
					}
					if (complete == "")
						complete = "i-have-a-lame-nick";
					newhost = complete + "." + MySuffix;
				}
				if (newhost != "")
				{
					Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :Setting your VHost: " + newhost);
					Srv->ChangeHost(user,newhost);
					return;
				}
			}
		}
	}
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleHostChangeFactory : public ModuleFactory
{
 public:
	ModuleHostChangeFactory()
	{
	}
	
	~ModuleHostChangeFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleHostChange;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleHostChangeFactory;
}

