/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
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

class ModuleSSLRehashSignal final
	: public Module
{
private:
	static void SignalHandler(int)
	{
		signaled = 1;
	}

public:
	 ModuleSSLRehashSignal()
		: Module(VF_VENDOR, "Allows the SIGUSR1 signal to be sent to the server to reload TLS certificates.")
	{
	}

	~ModuleSSLRehashSignal() override
	{
		signal(SIGUSR1, SIG_IGN);
	}

	void init() override
	{
		signal(SIGUSR1, SignalHandler);
	}

	void OnBackgroundTimer(time_t) override
	{
		if (!signaled)
			return;

		const std::string feedbackmsg = "Got SIGUSR1, reloading TLS credentials";
		ServerInstance->SNO.WriteGlobalSno('r', feedbackmsg);
		ServerInstance->Logs.Normal(MODNAME, feedbackmsg);

		const std::string str = "tls";
		FOREACH_MOD(OnModuleRehash, (nullptr, str));
		signaled = 0;
	}
};

MODULE_INIT(ModuleSSLRehashSignal)
