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
		flags_needed = 'o'; syntax = "<target> <modes> {<mode-parameters>}";
		active = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (parameters[0].c_str()[0] != '#')
		{
			User* target = ServerInstance->FindNickOnly(parameters[0]);
			if ((!target) || (target->registered != REG_ALL))
			{
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
				return CMD_FAILURE;
			}

			// Changing the modes of another user requires a special permission
			if ((target != user) && (!user->HasPrivPermission("users/samode-usermodes", true)))
				return CMD_FAILURE;
		}

		// XXX: Make ModeParser clear LastParse
		Modes::ChangeList emptychangelist;
		ServerInstance->Modes->ProcessSingle(ServerInstance->FakeClient, NULL, ServerInstance->FakeClient, emptychangelist);

		this->active = true;
		CmdResult result = ServerInstance->Parser.CallHandler("MODE", parameters, user);
		this->active = false;

		if (result == CMD_SUCCESS)
		{
			// If lastparse is empty and the MODE command handler returned CMD_SUCCESS then
			// the client queried the list of a listmode (e.g. /SAMODE #chan b), which was
			// handled internally by the MODE command handler.
			//
			// Viewing the modes of a user or a channel can also result in CMD_SUCCESS, but
			// that is not possible with /SAMODE because we require at least 2 parameters.
			const std::string& lastparse = ServerInstance->Modes.GetLastParse();
			ServerInstance->SNO->WriteGlobalSno('a', user->nick + " used SAMODE: " + (lastparse.empty() ? irc::stringjoiner(parameters) : lastparse));
		}

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

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides command SAMODE to allow opers to change modes on channels and users", VF_VENDOR);
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, Modes::ChangeList& modes) CXX11_OVERRIDE
	{
		if (cmd.active)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void Prioritize() CXX11_OVERRIDE
	{
		Module *override = ServerInstance->Modules->Find("m_override.so");
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_BEFORE, override);
	}
};

MODULE_INIT(ModuleSaMode)
