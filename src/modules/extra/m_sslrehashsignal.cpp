/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Peter Powell <petpow@saberuk.com>
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

static volatile sig_atomic_t signaled;

class ModuleSSLRehashSignal : public Module
{
 private:
	static void SignalHandler(int)
	{
		signaled = 1;
	}

 public:
	~ModuleSSLRehashSignal()
	{
		signal(SIGUSR1, SIG_IGN);
	}

	void init()
	{
		signal(SIGUSR1, SignalHandler);
	}

	void OnBackgroundTimer(time_t)
	{
		if (!signaled)
			return;

		const std::string feedbackmsg = "Got SIGUSR1, reloading SSL credentials";
		ServerInstance->SNO->WriteGlobalSno('a', feedbackmsg);
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, feedbackmsg);

		const std::string str = "ssl";
		FOREACH_MOD(OnModuleRehash, (NULL, str));
		signaled = 0;
	}

	Version GetVersion()
	{
		return Version("Reloads SSL credentials on SIGUSR1", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSSLRehashSignal)
