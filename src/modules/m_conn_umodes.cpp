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
#include <vector>
#include "users.h"
#include "channels.h"
#include "inspircd.h"
#include "modules.h"
#include "wildcard.h"

/* $ModDesc: Sets (and unsets) modes on users when they connect */

class ModuleModesOnConnect : public Module
{
 private:

	ConfigReader *Conf;

 public:
	ModuleModesOnConnect(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		Conf = new ConfigReader(ServerInstance);
	}

	void Implements(char* List)
	{
		List[I_OnPostConnect] = List[I_OnRehash] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		DELETE(Conf);
		Conf = new ConfigReader(ServerInstance);
	}
	
	virtual ~ModuleModesOnConnect()
	{
		DELETE(Conf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR,API_VERSION);
	}
	
	virtual void OnPostConnect(userrec* user)
	{
		if (!IS_LOCAL(user))
			return;

		ServerInstance->Log(DEBUG,"Post connect for mode setting");
		for (int j = 0; j < Conf->Enumerate("connect"); j++)
		{
			std::string hostn = Conf->ReadValue("connect","allow",j);
			if ((match(user->GetIPString(),hostn.c_str(),true)) || (match(user->host,hostn.c_str())))
			{
				ServerInstance->Log(DEBUG,"Found matching connect block '%s'",hostn.c_str());
				std::string ThisModes = Conf->ReadValue("connect","modes",j);
				if (ThisModes != "")
				{
					std::string buf;
					stringstream ss(ThisModes);

					vector<string> tokens;

					// split ThisUserModes into modes and mode params
					while (ss >> buf)
						tokens.push_back(buf);

					int size = tokens.size() + 1;
					const char* modes[size];
					modes[0] = user->nick;
					modes[1] = tokens[0].c_str();

					if (tokens.size() > 1)
					{
						// process mode params
						int i = 2;
						for (unsigned int k = 1; k < tokens.size(); k++)
						{
							modes[i] = tokens[k].c_str();
							i++;
						}
					}

					ServerInstance->SendMode(modes, size, user);
				}
				break;
			}
		}
	}
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleModesOnConnectFactory : public ModuleFactory
{
 public:
	ModuleModesOnConnectFactory()
	{
	}
	
	~ModuleModesOnConnectFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleModesOnConnect(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleModesOnConnectFactory;
}

