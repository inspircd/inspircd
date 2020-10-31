/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2010 Craig Edwards <brain@inspircd.org>
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

class ModuleConnFlood : public Module
{
 private:
	unsigned int seconds;
	unsigned int timeout;
	unsigned int boot_wait;
	unsigned int conns = 0;
	unsigned int maxconns;
	bool throttled = false;
	time_t first;
	std::string quitmsg;

 public:
	ModuleConnFlood()
		: Module(VF_VENDOR, "Throttles IP addresses which make excessive connections to the server.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		/* read configuration variables */
		auto tag = ServerInstance->Config->ConfValue("connflood");
		/* throttle configuration */
		seconds = tag->getDuration("period", 30);
		maxconns = tag->getUInt("maxconns", 3);
		timeout = tag->getDuration("timeout", 30);
		quitmsg = tag->getString("quitmsg");

		/* seconds to wait when the server just booted */
		boot_wait = tag->getDuration("bootwait", 60*2);

		first = ServerInstance->Time();
	}

	ModResult OnUserRegister(LocalUser* user) override
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
				ServerInstance->SNO.WriteGlobalSno('a', "Connection throttle deactivated");
				return MOD_RES_PASSTHRU;
			}

			ServerInstance->Users.QuitUser(user, quitmsg);
			return MOD_RES_DENY;
		}

		if (tdiff <= seconds)
		{
			if (conns >= maxconns)
			{
				throttled = true;
				ServerInstance->SNO.WriteGlobalSno('a', "Connection throttle activated");
				ServerInstance->Users.QuitUser(user, quitmsg);
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
};

MODULE_INIT(ModuleConnFlood)
