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

class ModuleOperLog : public Module
{
	bool tosnomask;

 public:
	void init() CXX11_OVERRIDE
	{
		ServerInstance->SNO->EnableSnomask('r', "OPERLOG");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("A module which logs all oper commands to the ircd log at default loglevel.", VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		tosnomask = ServerInstance->Config->ConfValue("operlog")->getBool("tosnomask", false);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((user->IsOper()) && (user->HasPermission(command)))
		{
			Command* thiscommand = ServerInstance->Parser.GetHandler(command);
			if ((thiscommand) && (thiscommand->flags_needed == 'o'))
			{
				std::string msg = "[" + user->GetFullRealHost() + "] " + command + " " + irc::stringjoiner(parameters);
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "OPERLOG: " + msg);
				if (tosnomask)
					ServerInstance->SNO->WriteGlobalSno('r', msg);
			}
		}

		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["OPERLOG"];
	}

};

MODULE_INIT(ModuleOperLog)
