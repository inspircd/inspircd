/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
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

enum
{
	// From ircu.
	ERR_DISABLED = 517,

	// InspIRCd-specific.
	RPL_COMMANDS = 700
};

// Holds a list of disabled commands.
typedef std::vector<std::string> CommandList;

class ModuleDisable final
	: public Module
{
private:
	CommandList commands;
	ModeParser::ModeStatus chanmodes;
	bool fakenonexistent;
	bool notifyopers;
	ModeParser::ModeStatus usermodes;

	void ReadModes(std::shared_ptr<ConfigTag> tag, const std::string& field, ModeType type, ModeParser::ModeStatus& status)
	{
		for (const auto& chr : tag->getString(field))
		{
			// Check that the character is a valid mode letter.
			if (!ModeParser::IsModeChar(chr))
				throw ModuleException(this, InspIRCd::Format("Invalid mode '%c' was specified in <disabled:%s> at %s",
					chr, field.c_str(), tag->source.str().c_str()));

			// Check that the mode actually exists.
			ModeHandler* mh = ServerInstance->Modes.FindMode(chr, type);
			if (!mh)
				throw ModuleException(this, InspIRCd::Format("Nonexistent mode '%c' was specified in <disabled:%s> at %s",
					chr, field.c_str(), tag->source.str().c_str()));

			// Disable the mode.
			ServerInstance->Logs.Debug(MODNAME, "The %c (%s) %s mode has been disabled",
				mh->GetModeChar(), mh->name.c_str(), type == MODETYPE_CHANNEL ? "channel" : "user");
			status.set(ModeParser::GetModeIndex(chr));
		}
	}

	void WriteLog(const char* message, ...) const ATTR_PRINTF(2, 3)
	{
		std::string buffer;
		VAFORMAT(buffer, message, message);

		if (notifyopers)
			ServerInstance->SNO.WriteToSnoMask('a', buffer);
		else
			ServerInstance->Logs.Normal(MODNAME, buffer);
	}

public:
	ModuleDisable()
		: Module(VF_VENDOR, "Allows commands, channel modes, and user modes to be disabled.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto tag = ServerInstance->Config->ConfValue("disabled");

		// Parse the disabled commands.
		CommandList newcommands;
		irc::spacesepstream commandlist(tag->getString("commands"));
		for (std::string command; commandlist.GetToken(command); )
		{
			// Check that the command actually exists.
			Command* handler = ServerInstance->Parser.GetHandler(command);
			if (!handler)
				throw ModuleException(this, InspIRCd::Format("Nonexistent command '%s' was specified in <disabled:commands> at %s",
					command.c_str(), tag->source.str().c_str()));

			// Prevent admins from disabling MODULES for transparency reasons.
			if (handler->name == "MODULES")
				continue;

			// Disable the command.
			ServerInstance->Logs.Debug(MODNAME, "The %s command has been disabled", handler->name.c_str());
			newcommands.push_back(handler->name);
		}

		// Parse the disabled channel modes.
		ModeParser::ModeStatus newchanmodes;
		ReadModes(tag, "chanmodes", MODETYPE_CHANNEL, newchanmodes);

		// Parse the disabled user modes.
		ModeParser::ModeStatus newusermodes;
		ReadModes(tag, "usermodes", MODETYPE_USER, newusermodes);

		// The server config was valid so we can use these now.
		chanmodes = newchanmodes;
		usermodes = newusermodes;
		commands.swap(newcommands);

		// Whether we should fake the non-existence of disabled things.
		fakenonexistent = tag->getBool("fakenonexistent");

		// Whether to notify server operators via snomask `a` about the attempted use of disabled commands/modes.
		notifyopers = tag->getBool("notifyopers");
	}

	ModResult OnNumeric(User* user, const Numeric::Numeric& numeric) override
	{
		if (numeric.GetNumeric() != RPL_COMMANDS || numeric.GetParams().empty())
			return MOD_RES_PASSTHRU; // The numeric isn't the one we care about.

		if (!fakenonexistent || !IS_LOCAL(user))
			return MOD_RES_PASSTHRU; // We're not hiding the numeric OR the user is remote.

		if (!stdalgo::isin(commands, numeric.GetParams()[0]) || user->HasPrivPermission("servers/use-disabled-commands"))
			return MOD_RES_PASSTHRU; // Wrong command or the user is is an exempt oper.

		return MOD_RES_DENY;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// If a command is unvalidated or the source is not registered we do nothing.
		if (!validated || !user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		// If the command is not disabled or the user has the servers/use-disabled-commands priv we do nothing.
		if (!stdalgo::isin(commands, command) || user->HasPrivPermission("servers/use-disabled-commands"))
			return MOD_RES_PASSTHRU;

		// The user has tried to execute a disabled command!
		user->CommandFloodPenalty += 2000;
		WriteLog("%s was blocked from executing the disabled %s command", user->GetFullRealHost().c_str(), command.c_str());

		if (fakenonexistent)
		{
			// The server administrator has specified that disabled commands should be
			// treated as if they do not exist.
			user->WriteNumeric(ERR_UNKNOWNCOMMAND, command, "Unknown command");
			ServerInstance->stats.Unknown++;
			return MOD_RES_DENY;
		}

		// Inform the user that the command they executed has been disabled.
		user->WriteNumeric(ERR_DISABLED, command, "Command disabled");
		return MOD_RES_DENY;
	}

	ModResult OnRawMode(User* user, Channel* chan, const Modes::Change& change) override
	{
		// If a mode change is remote or the source is not registered we do nothing.
		if (!IS_LOCAL(user) || !user->IsFullyConnected())
			return MOD_RES_PASSTHRU;

		// If the mode is not disabled or the user has the servers/use-disabled-modes priv we do nothing.
		const ModeParser::ModeStatus& disabled = (change.mh->GetModeType() == MODETYPE_CHANNEL) ? chanmodes : usermodes;
		if (!disabled.test(ModeParser::GetModeIndex(change.mh->GetModeChar())) || user->HasPrivPermission("servers/use-disabled-modes"))
			return MOD_RES_PASSTHRU;

		// The user has tried to change a disabled mode!
		const char* what = change.mh->GetModeType() == MODETYPE_CHANNEL ? "channel" : "user";
		WriteLog("%s was blocked from %ssetting the disabled %s mode %c (%s)",
			user->GetFullRealHost().c_str(), change.adding ? "" : "un",
			what, change.mh->GetModeChar(), change.mh->name.c_str());

		if (fakenonexistent)
		{
			// The server administrator has specified that disabled modes should be
			// treated as if they do not exist.
			int numeric = (change.mh->GetModeType() == MODETYPE_CHANNEL ? ERR_UNKNOWNMODE : ERR_UNKNOWNSNOMASK);
			const char* typestr = (change.mh->GetModeType() == MODETYPE_CHANNEL ? "channel" : "user");
			user->WriteNumeric(numeric, change.mh->GetModeChar(), InspIRCd::Format("is not a recognised %s mode.", typestr));
			return MOD_RES_DENY;
		}

		// Inform the user that the mode they changed has been disabled.
		user->WriteNumeric(ERR_NOPRIVILEGES, InspIRCd::Format("Permission Denied - %s mode %c (%s) is disabled",
			what, change.mh->GetModeChar(), change.mh->name.c_str()));
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleDisable)
