/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"
#include "utility/string.h"

class ModuleOperLog final
	: public Module
{
private:
	bool tosnomask;

public:
	ModuleOperLog()
		: Module(VF_VENDOR, "Allows the server administrator to make the server log when a server operator-only command is executed.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		tosnomask = ServerInstance->Config->ConfValue("operlog")->getBool("tosnomask", false);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return MOD_RES_PASSTHRU;

		if ((user->IsOper()) && (user->HasCommandPermission(command)))
		{
			Command* thiscommand = ServerInstance->Parser.GetHandler(command);
			if ((thiscommand) && (thiscommand->access_needed == CmdAccess::OPERATOR))
			{
				std::string msg = "[" + user->GetRealMask() + "] " + command + " " + insp::join(parameters);
				if (tosnomask)
					ServerInstance->SNO.WriteGlobalSno('o', msg);
				else
					ServerInstance->Logs.Normal(MODNAME, msg);
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleOperLog)
