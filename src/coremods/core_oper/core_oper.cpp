/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "core_oper.h"

class CoreModOper : public Module
{
	CommandDie cmddie;
	CommandKill cmdkill;
	CommandOper cmdoper;
	CommandRehash cmdrehash;
	CommandRestart cmdrestart;

 public:
	CoreModOper()
		: cmddie(this), cmdkill(this), cmdoper(this), cmdrehash(this), cmdrestart(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* security = ServerInstance->Config->ConfValue("security");
		std::string hidenick = security->getString("hidekills");
		bool hideuline = security->getBool("hideulinekills");

		ConfigTag* tag = ServerInstance->Config->ConfValue("power");
		// The hash method for *BOTH* the die and restart passwords
		const std::string hash = tag->getString("hash");
		const std::string diepass = tag->getString("diepass", ServerInstance->Config->ServerName);
		const std::string restartpass = tag->getString("restartpass", ServerInstance->Config->ServerName);

		cmddie.hash = hash;
		cmddie.password = diepass;

		cmdrestart.hash = hash;
		cmdrestart.password = restartpass;

		cmdkill.hidenick = hidenick;
		cmdkill.hideuline = hideuline;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the DIE, KILL, OPER, REHASH, and RESTART commands", VF_VENDOR | VF_CORE);
	}
};

MODULE_INIT(CoreModOper)
