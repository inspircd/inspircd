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

/* $ModDesc: A module which logs all oper commands to the ircd log at default loglevel. */

class ModuleOperLog : public Module
{
	bool tosnomask;

 public:
	void init()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->SNO->EnableSnomask('r', "OPERLOG");
		OnRehash(NULL);
	}

	virtual ~ModuleOperLog()
	{
	}

	virtual Version GetVersion()
	{
		return Version("A module which logs all oper commands to the ircd log at default loglevel.", VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		tosnomask = ServerInstance->Config->ConfValue("operlog")->getBool("tosnomask", false);
	}

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((IS_OPER(user)) && (IS_LOCAL(user)) && (user->HasPermission(command)))
		{
			Command* thiscommand = ServerInstance->Parser->GetHandler(command);
			if ((thiscommand) && (thiscommand->flags_needed == 'o'))
			{
				std::string line;
				if (!parameters.empty())
					line = irc::stringjoiner(" ", parameters, 0, parameters.size() - 1).GetJoined();
				std::string msg = "[" + user->GetFullRealHost() + "] " + command + " " + line;
				ServerInstance->Logs->Log("m_operlog", DEFAULT, "OPERLOG: " + msg);
				if (tosnomask)
					ServerInstance->SNO->WriteGlobalSno('r', msg);
			}
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" OPERLOG");
	}

};


MODULE_INIT(ModuleOperLog)
