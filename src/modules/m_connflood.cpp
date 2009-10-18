/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Connection throttle */

int conns = 0, throttled = 0;

class ModuleConnFlood : public Module
{
private:
	int seconds, maxconns, timeout, boot_wait;
	time_t first;
	std::string quitmsg;

public:
	ModuleConnFlood() 	{

		InitConf();
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleConnFlood()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Connection throttle", VF_VENDOR);
	}

	void InitConf()
	{
		/* read configuration variables */
		ConfigReader conf;
		/* throttle configuration */
		seconds = conf.ReadInteger("connflood", "seconds", 0, true);
		maxconns = conf.ReadInteger("connflood", "maxconns", 0, true);
		timeout = conf.ReadInteger("connflood", "timeout", 0, true);
		quitmsg = conf.ReadValue("connflood", "quitmsg", 0);

		/* seconds to wait when the server just booted */
		boot_wait = conf.ReadInteger("connflood", "bootwait", 0, true);

		first = ServerInstance->Time();
	}

	virtual ModResult OnUserRegister(User* user)
	{
		time_t next = ServerInstance->Time();

		if ((ServerInstance->startup_time + boot_wait) > next)
			return MOD_RES_PASSTHRU;

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
				ServerInstance->SNO->WriteGlobalSno('a', "Connection throttle deactivated");
				return MOD_RES_PASSTHRU;
			}

			ServerInstance->Users->QuitUser(user, quitmsg);
			return MOD_RES_DENY;
		}

		if (tdiff <= seconds)
		{
			if (conns >= maxconns)
			{
				throttled = 1;
				ServerInstance->SNO->WriteGlobalSno('a', "Connection throttle activated");
				ServerInstance->Users->QuitUser(user, quitmsg);
				return MOD_RES_DENY;
			}
		}
		else
		{
			conns = 1;
			first = next;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		InitConf();
	}

};

MODULE_INIT(ModuleConnFlood)
