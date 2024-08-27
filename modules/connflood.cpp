/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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

class ModuleConnFlood final
	: public Module
{
private:
	unsigned long seconds;
	unsigned long timeout;
	unsigned long boot_wait;
	unsigned long conns = 0;
	unsigned long maxconns;
	bool throttled = false;
	time_t first;
	std::string quitmsg;

	static bool IsExempt(LocalUser* user)
	{
		// E-lined and already banned users shouldn't be hit.
		if (user->exempt || user->quitting)
			return true;

		// Users in an exempt class shouldn't be hit.
		return user->GetClass() && !user->GetClass()->config->getBool("useconnflood", true);
	}

public:
	ModuleConnFlood()
		: Module(VF_VENDOR, "Throttles excessive connections to the server.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		/* read configuration variables */
		const auto& tag = ServerInstance->Config->ConfValue("connflood");
		/* throttle configuration */
		seconds = tag->getDuration("period", 30);
		maxconns = tag->getNum<unsigned long>("maxconns", 3);
		timeout = tag->getDuration("timeout", 30);
		quitmsg = tag->getString("quitmsg");

		/* seconds to wait when the server just booted */
		boot_wait = tag->getDuration("bootwait", 60*2);

		first = ServerInstance->Time();
	}

	ModResult OnUserRegister(LocalUser* user) override
	{
		if (IsExempt(user))
			return MOD_RES_PASSTHRU;

		time_t next = ServerInstance->Time();
		if ((time_t)(ServerInstance->StartTime + boot_wait) > next)
			return MOD_RES_PASSTHRU;

		/* time difference between first and latest connection */
		unsigned long tdiff = next - first;

		/* increase connection count */
		conns++;

		if (throttled)
		{
			if (tdiff > (seconds + timeout))
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
