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
#include "inspircd.h"
#include "modules.h"

/* $ModDesc: Sets (and unsets) modes on opers when they oper up */

class ModuleModesOnOper : public Module
{
 private:

	
	ConfigReader *Conf;

 public:
	ModuleModesOnOper(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		Conf = new ConfigReader(ServerInstance);
	}

	void Implements(char* List)
	{
		List[I_OnPostOper] = List[I_OnRehash] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		DELETE(Conf);
		Conf = new ConfigReader(ServerInstance);
	}
	
	virtual ~ModuleModesOnOper()
	{
		DELETE(Conf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
	virtual void OnPostOper(userrec* user, const std::string &opertype)
	{
		// whenever a user opers, go through the oper types, find their <type:modes>,
		// and if they have one apply their modes. The mode string can contain +modes
		// to add modes to the user or -modes to take modes from the user.
		for (int j =0; j < Conf->Enumerate("type"); j++)
		{
			std::string typen = Conf->ReadValue("type","name",j);
			if (!strcmp(typen.c_str(),user->oper))
			{
				std::string ThisOpersModes = Conf->ReadValue("type","modes",j);
				char first = *(ThisOpersModes.c_str());
				if ((first != '+') && (first != '-'))
					ThisOpersModes = "+" + ThisOpersModes;
				if (ThisOpersModes != "")
				{
					const char* modes[2];
					modes[0] = user->nick;
					modes[1] = ThisOpersModes.c_str();
					ServerInstance->SendMode(modes,2,user);
				}
				break;
			}
		}
	}
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleModesOnOperFactory : public ModuleFactory
{
 public:
	ModuleModesOnOperFactory()
	{
	}
	
	~ModuleModesOnOperFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleModesOnOper(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleModesOnOperFactory;
}

