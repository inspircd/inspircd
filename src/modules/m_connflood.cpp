/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/* $ModDesc: Connection throttle */

class ModuleConnFlood : public Module
{
private:
	int seconds, timeout, boot_wait;
	unsigned int conns;
	unsigned int maxconns;
	bool throttled;
	time_t first;
	std::string quitmsg;

public:
	ModuleConnFlood()
		: conns(0), throttled(false)
	{
	}

	void init()
	{
		InitConf();
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual Version GetVersion()
	{
		return Version("Connection throttle", VF_VENDOR);
	}

	void InitConf()
	{
		/* read configuration variables */
		ConfigTag* tag = ServerInstance->Config->ConfValue("connflood");
		/* throttle configuration */
		seconds = tag->getInt("seconds");
		maxconns = tag->getInt("maxconns");
		timeout = tag->getInt("timeout");
		quitmsg = tag->getString("quitmsg");

		/* seconds to wait when the server just booted */
		boot_wait = tag->getInt("bootwait");

		first = ServerInstance->Time();
	}

	virtual ModResult OnUserRegister(LocalUser* user)
	{
		if (user->exempt)
			return MOD_RES_PASSTHRU;

		time_t next = ServerInstance->Time();

		if ((ServerInstance->startup_time + boot_wait) > next)
			return MOD_RES_PASSTHRU;

		/* time difference between first and latest connection */
		time_t tdiff = next - first;

		/* increase connection count */
		conns++;

		if (throttled)
		{
			if (tdiff > seconds + timeout)
			{
				/* expire throttle */
				throttled = false;
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
				throttled = true;
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
