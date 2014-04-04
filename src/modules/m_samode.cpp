/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005 Craig Edwards <craigedwards@brainbox.cc>
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


/* $ModDesc: Provides command SAMODE to allow opers to change modes on channels and users */

#include "inspircd.h"

/** Handle /SAMODE
 */
class CommandSamode : public Command
{
 public:
	bool active;
	CommandSamode(Module* Creator) : Command(Creator,"SAMODE", 2)
	{
		allow_empty_last_param = false;
		flags_needed = 'o'; Penalty = 0; syntax = "<target> <modes> {<mode-parameters>}";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (parameters[0].c_str()[0] != '#')
		{
			User* target = ServerInstance->FindNickOnly(parameters[0]);
			if ((!target) || (target->registered != REG_ALL))
			{
				user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}

		this->active = true;
		ServerInstance->Parser->CallHandler("MODE", parameters, user);
		if (ServerInstance->Modes->GetLastParse().length())
			ServerInstance->SNO->WriteGlobalSno('a', user->nick + " used SAMODE: " +ServerInstance->Modes->GetLastParse());
		this->active = false;
		return CMD_SUCCESS;
	}
};

class ModuleSaMode : public Module
{
	CommandSamode cmd;
 public:
	ModuleSaMode()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->Attach(I_OnPreMode, this);
	}

	~ModuleSaMode()
	{
	}

	Version GetVersion()
	{
		return Version("Provides command SAMODE to allow opers to change modes on channels and users", VF_VENDOR);
	}

	ModResult OnPreMode(User* source,User* dest,Channel* channel, const std::vector<std::string>& parameters)
	{
		if (cmd.active)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void Prioritize()
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_BEFORE, &override);
	}
};

MODULE_INIT(ModuleSaMode)
