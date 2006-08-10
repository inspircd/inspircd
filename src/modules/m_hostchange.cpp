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
#include "inspircd.h"

/* $ModDesc: Provides masking of user hostnames in a different way to m_cloaking */

extern InspIRCd* ServerInstance;

class Host : public classbase
{
 public:
	std::string action;
	std::string newhost;
};

typedef std::map<std::string,Host*> hostchanges_t;

class ModuleHostChange : public Module
{
 private:

	
	ConfigReader *Conf;
	hostchanges_t hostchanges;
	std::string MySuffix;
	 
 public:
	ModuleHostChange(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		Conf = new ConfigReader;
		OnRehash("");
	}
	
	virtual ~ModuleHostChange()
	{
		DELETE(Conf);
	}

	Priority Prioritize()
	{
		return (Priority)ServerInstance->PriorityAfter("m_cloaking.so");
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserConnect] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		DELETE(Conf);
		Conf = new ConfigReader;
		MySuffix = Conf->ReadValue("host","suffix",0);
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			DELETE(i->second);
		}
		hostchanges.clear();
		for (int index = 0; index < Conf->Enumerate("hostchange"); index++)
		{
			std::string mask = Conf->ReadValue("hostchange","mask",index);
			std::string action = Conf->ReadValue("hostchange","action",index);
			std::string newhost = Conf->ReadValue("hostchange","value",index);
			Host* x = new Host;
			x->action = action;
			x->newhost = newhost;
			hostchanges[mask] = x;
		}
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,0,0,1,VF_VENDOR);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		for (hostchanges_t::iterator i = hostchanges.begin(); i != hostchanges.end(); i++)
		{
			if (ServerInstance->MatchText(std::string(user->ident)+"@"+std::string(user->host),i->first))
			{
				Host* h = (Host*)i->second;
				// host of new user matches a hostchange tag's mask
				std::string newhost = "";
				if (h->action == "set")
				{
					newhost = h->newhost;
				}
				else if (h->action == "suffix")
				{
					newhost = MySuffix;
				}
				else if (h->action == "addnick")
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
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Setting your virtual host: " + newhost);
					if (!user->ChangeDisplayedHost(newhost.c_str()))
						user->WriteServ("NOTICE "+std::string(user->nick)+" :Could not set your virtual host: " + newhost);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleHostChange(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleHostChangeFactory;
}

