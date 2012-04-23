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

int conns = 0, throttled = 0;

class ModuleConnFlood : public Module
{
private:
	int seconds, maxconns, timeout, boot_wait;
	time_t first;
	std::string quitmsg;

	ConfigReader* conf;


public:
	ModuleConnFlood(InspIRCd* Me) : Module(Me)
	{

		InitConf();
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleConnFlood()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR,API_VERSION);
	}

	void InitConf()
	{
		/* read configuration variables */
		conf = new ConfigReader(ServerInstance);
		/* throttle configuration */
		seconds = conf->ReadInteger("connflood", "seconds", 0, true);
		maxconns = conf->ReadInteger("connflood", "maxconns", 0, true);
		timeout = conf->ReadInteger("connflood", "timeout", 0, true);
		quitmsg = conf->ReadValue("connflood", "quitmsg", 0);

		/* seconds to wait when the server just booted */
		boot_wait = conf->ReadInteger("connflood", "bootwait", 0, true);

		first = ServerInstance->Time();
	}

	virtual int OnUserRegister(User* user)
	{
		time_t next = ServerInstance->Time();

		if ((ServerInstance->startup_time + boot_wait) > next)
			return 0;

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
				return 0;
			}

			ServerInstance->Users->QuitUser(user, quitmsg);
			return 1;
		}

		if (tdiff <= seconds)
		{
			if (conns >= maxconns)
			{
				throttled = 1;
				ServerInstance->SNO->WriteGlobalSno('a', "Connection throttle activated");
				ServerInstance->Users->QuitUser(user, quitmsg);
				return 1;
			}
		}
		else
		{
			conns = 1;
			first = next;
		}
		return 0;
	}

	virtual void OnRehash(User* user)
	{
		InitConf();
	}

};

MODULE_INIT(ModuleConnFlood)
