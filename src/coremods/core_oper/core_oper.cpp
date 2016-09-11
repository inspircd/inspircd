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

namespace DieRestart
{
	bool CheckPass(User* user, const std::string& inputpass, const char* confentry)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("power");
		// The hash method for *BOTH* the die and restart passwords
		const std::string hash = tag->getString("hash");
		const std::string correctpass = tag->getString(confentry,  ServerInstance->Config->ServerName);
		return ServerInstance->PassCompare(user, correctpass, inputpass, hash);
	}
}

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

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the DIE, KILL, OPER, REHASH, and RESTART commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(CoreModOper)
