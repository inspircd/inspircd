/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "clientprotocolmsg.h"
#include "numerichelper.h"
#include "utility/string.h"

class CommandSamode final
	: public Command
{
	bool logged;

public:
	bool active;
	CommandSamode(Module* Creator)
		: Command(Creator, "SAMODE", 2)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<target> (+|-)<modes> [<mode-parameters>]" };
		active = false;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!ServerInstance->Channels.IsPrefix(parameters[0][0]))
		{
			auto* target = ServerInstance->Users.FindNick(parameters[0], true);
			if (!target)
			{
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
				return CmdResult::FAILURE;
			}

			// Changing the modes of another user requires a special permission
			if ((target != user) && (!user->HasPrivPermission("users/samode-usermodes")))
			{
				user->WriteNotice("*** You are not allowed to /SAMODE other users (the privilege users/samode-usermodes is needed to /SAMODE others).");
				return CmdResult::FAILURE;
			}
		}

		// XXX: Make ModeParser clear LastParse
		Modes::ChangeList emptychangelist;
		ServerInstance->Modes.ProcessSingle(ServerInstance->FakeClient, nullptr, ServerInstance->FakeClient, emptychangelist);

		logged = false;
		this->active = true;
		ServerInstance->Parser.CallHandler("MODE", parameters, user);
		this->active = false;

		if (!logged)
		{
			// If we haven't logged anything yet then the client queried the list of a listmode
			// (e.g. /SAMODE #chan b), which was handled internally by the MODE command handler.
			//
			// Viewing the modes of a user or a channel could also result in this, but
			// that is not possible with /SAMODE because we require at least 2 parameters.
			LogUsage(user, insp::join(parameters));
		}

		return CmdResult::SUCCESS;
	}

	void LogUsage(const User* user, const std::string& text)
	{
		logged = true;
		ServerInstance->SNO.WriteGlobalSno('a', user->nick + " used SAMODE: " + text);
	}
};

class ModuleSaMode final
	: public Module
{
private:
	CommandSamode cmd;

public:
	ModuleSaMode()
		: Module(VF_VENDOR, "Adds the /SAMODE command which allows server operators to change the modes of a target (channel, user) that they would not otherwise have the privileges to change.")
		, cmd(this)
	{
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, Modes::ChangeList& modes) override
	{
		if (cmd.active)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	void OnMode(User* user, User* destuser, Channel* destchan, const Modes::ChangeList& modes, ModeParser::ModeProcessFlag processflags) override
	{
		if (!cmd.active)
			return;

		std::string logtext = (destuser ? destuser->nick : destchan->name);
		logtext.push_back(' ');
		logtext += ClientProtocol::Messages::Mode::ToModeLetters(modes);

		for (const auto& item : modes.getlist())
		{
			if (!item.param.empty())
				logtext.append(1, ' ').append(item.param);
		}

		cmd.LogUsage(user, logtext);
	}

	void Prioritize() override
	{
		Module* disable = ServerInstance->Modules.Find("disable");
		ServerInstance->Modules.SetPriority(this, I_OnRawMode, PRIORITY_BEFORE, disable);

		Module* override = ServerInstance->Modules.Find("override");
		ServerInstance->Modules.SetPriority(this, I_OnPreMode, PRIORITY_BEFORE, override);
	}
};

MODULE_INIT(ModuleSaMode)
