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
#include "inspircd.h"

/* $ModDesc: Only opers may create new channels if this module is loaded */

class ModuleRestrictChans : public Module
{
	

	std::map<irc::string,int> allowchans;

	void ReadConfig()
	{
		ConfigReader* MyConf = new ConfigReader();
		allowchans.clear();
		for (int i = 0; i < MyConf->Enumerate("allowchannel"); i++)
		{
			std::string txt;
			txt = MyConf->ReadValue("allowchannel", "name", i);
			irc::string channel = txt.c_str();
			allowchans[channel] = 1;
		}
		DELETE(MyConf);
	}

 public:
	ModuleRestrictChans(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		ReadConfig();
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ReadConfig();
	}

	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = List[I_OnRehash] = 1;
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		irc::string x = cname;
		// user is not an oper and its not in the allow list
		if ((!*user->oper) && (allowchans.find(x) == allowchans.end()))
		{
			// channel does not yet exist (record is null, about to be created IF we were to allow it)
			if (!chan)
			{
				user->WriteServ("530 %s %s :Only IRC operators may create new channels",user->nick,cname,cname);
				return 1;
			}
		}
		return 0;
	}
	
    	virtual ~ModuleRestrictChans()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
};


class ModuleRestrictChansFactory : public ModuleFactory
{
 public:
	ModuleRestrictChansFactory()
	{
	}
	
	~ModuleRestrictChansFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleRestrictChans(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRestrictChansFactory;
}

