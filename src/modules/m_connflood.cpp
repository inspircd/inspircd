/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                    E-mail:
 *             <brain@chatspike.net>
 *             <Craig@chatspike.net>
 *
 *     --- This module contributed by pippijn ---
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 * the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Connection throttle */

int conns = 0, throttled = 0;
extern time_t TIME;

extern InspIRCd* ServerInstance;

class ModuleConnFlood : public Module
{
private:
	int seconds, maxconns, timeout, boot_wait;
	time_t first;
	std::string quitmsg;

	ConfigReader* conf;
	Server *Srv;

public:
	ModuleConnFlood(InspIRCd* Me) : Module::Module(Me)
	{
		
		InitConf();
	}

	virtual ~ModuleConnFlood()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,0);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = 1;
	}
   
	void InitConf()
	{
		/* read configuration variables */
		conf = new ConfigReader;
		/* throttle configuration */
		seconds = conf->ReadInteger("connflood", "seconds", 0, true);
		maxconns = conf->ReadInteger("connflood", "maxconns", 0, true);
		timeout = conf->ReadInteger("connflood", "timeout", 0, true);
		quitmsg = conf->ReadValue("connflood", "quitmsg", 0);

		/* seconds to wait when the server just booted */
		boot_wait = conf->ReadInteger("connflood", "bootwait", 0, true);

		first = TIME;
	}
 
	virtual void OnUserRegister(userrec* user)
	{
		time_t next = TIME;
		if (!first)
			first = next - boot_wait;

		/* time difference between first and latest connection */
		time_t tdiff = next - first;

		/* increase connection count */
		conns++;

		if (throttled == 1)
		{
			if (tdiff > seconds + timeout)
			{
				/* expire throttle */
				throttled = 0;
				ServerInstance->WriteOpers("*** Connection throttle deactivated");
				return;
			}
			userrec::QuitUser(ServerInstance, user, quitmsg);
			return;
		}

		if (tdiff <= seconds)
		{
			if (conns >= maxconns)
			{
				throttled = 1;
				ServerInstance->WriteOpers("*** Connection throttle activated");
				userrec::QuitUser(ServerInstance, user, quitmsg);
				return;
			}
		}
		else
		{
			conns = 1;
			first = next;
		}
	}

	virtual void OnRehash(const std::string &parameter)
	{
		InitConf();
	}

};


class ModuleConnFloodFactory : public ModuleFactory
{
public:
	ModuleConnFloodFactory()
	{
	}

	~ModuleConnFloodFactory()
	{
	}
    
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleConnFlood(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleConnFloodFactory;
}
