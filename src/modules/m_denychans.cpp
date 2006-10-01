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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"

#include "inspircd.h"

/* $ModDesc: Implements config tags which allow blocking of joins to channels */

class ModuleDenyChannels : public Module
{
 private:

	
	ConfigReader *Conf;

 public:
	ModuleDenyChannels(InspIRCd* Me) : Module::Module(Me)
	{
		
		Conf = new ConfigReader(ServerInstance);
	}
	
	virtual void OnRehash(const std::string &param)
	{
		DELETE(Conf);
		Conf = new ConfigReader(ServerInstance);
	}
	
	virtual ~ModuleDenyChannels()
	{
		DELETE(Conf);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR,API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = List[I_OnRehash] = 1;
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname, std::string &privs)
	{
		for (int j =0; j < Conf->Enumerate("badchan"); j++)
		{
			irc::string cn = Conf->ReadValue("badchan","name",j).c_str();
			irc::string thischan = cname;
			if (thischan == cn)
			{
				if ((Conf->ReadFlag("badchan","allowopers",j)) && *user->oper)
				{
					return 0;
				}
				else
				{
					std::string reason = Conf->ReadValue("badchan","reason",j);
					user->WriteServ("926 %s %s :Channel %s is forbidden: %s",user->nick,cname,cname,reason.c_str());
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleDenyChannels(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleDenyChannelsFactory;
}

