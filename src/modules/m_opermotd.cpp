/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
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

enum
{
	// From UnrealIRCd.
	ERR_NOOPERMOTD = 425,

	// From ircd-ratbox.
	RPL_OMOTDSTART = 720,
	RPL_OMOTD = 721,
	RPL_ENDOFOMOTD = 722
};

typedef insp::flat_map<std::string, std::vector<std::string>> MOTDCache;

class CommandOperMOTD final
	: public Command
{
public:
	std::string file;
	MOTDCache motds;

	CommandOperMOTD(Module* Creator)
		: Command(Creator, "OPERMOTD")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<servername>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters.empty() || irc::equals(parameters[0], ServerInstance->Config->ServerName))
			return ShowOperMOTD(user, true);
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		if ((!parameters.empty()) && (parameters[0].find('.') != std::string::npos))
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}

	CmdResult ShowOperMOTD(User* user, bool showmissing)
	{
		if (!user->IsOper())
			return CmdResult::SUCCESS; // WTF?

		auto motd = motds.find(user->oper->GetConfig()->getString("motd", file, 1));
		if (motd == motds.end())
		{
			if (showmissing)
				user->WriteRemoteNumeric(ERR_NOOPERMOTD, "There is no server operator MOTD.");
			return CmdResult::SUCCESS;
		}

		user->WriteRemoteNumeric(RPL_OMOTDSTART, "Server operator MOTD:");
		for (const auto& line : motd->second)
			user->WriteRemoteNumeric(RPL_OMOTD, line);
		user->WriteRemoteNumeric(RPL_ENDOFOMOTD, "End of server operator MOTD.");

		return CmdResult::SUCCESS;
	}
};

class ModuleOperMOTD final
	: public Module
{
private:
	CommandOperMOTD cmd;
	bool onoper;

	void ProcessMOTD(MOTDCache& newmotds, const std::shared_ptr<OperType>& oper, const char* type) const
	{
		// Don't process the file if it has already been processed.
		const std::string motd = oper->GetConfig()->getString("motd", cmd.file);
		if (motd.empty() || newmotds.find(motd) != newmotds.end())
			return;

		auto file = ServerInstance->Config->ReadFile(motd);
		if (!file)
		{
			// We can't process the file if it doesn't exist.
			ServerInstance->Logs.Normal(MODNAME, "Unable to read server operator motd for oper {} \"{}\" at {}: {}",
				type, oper->GetName(), oper->GetConfig()->source.str(), file.error);
			return;
		}

		// Process the MOTD entry.
		auto& newmotd = newmotds[motd];
		irc::sepstream linestream(file.contents, '\n', true);
		for (std::string line; linestream.GetToken(line); )
		{
			// Some clients can not handle receiving RPL_OMOTD with an empty
			// trailing parameter so if a line is empty we replace it with
			// a single space.
			InspIRCd::ProcessColors(line);
			newmotd.push_back(line.empty() ? " " : line);
		}
	}

public:
	ModuleOperMOTD()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /OPERMOTD command which adds a special message of the day for server operators.")
		, cmd(this)
	{
	}

	void OnPostOperLogin(User* user, bool automatic) override
	{
		if (IS_LOCAL(user) && user->oper->GetConfig()->getBool("automotd", onoper))
			cmd.ShowOperMOTD(user, false);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// Compatibility with the v3 config.
		const auto& tag = ServerInstance->Config->ConfValue("opermotd");
		cmd.file = tag->getString("file", "opermotd", 1);
		onoper = tag->getBool("onoper", true);

		MOTDCache newmotds;
		for (const auto& [_, account] : ServerInstance->Config->OperAccounts)
			ProcessMOTD(newmotds, account, "account");
		for (const auto& [_, type] : ServerInstance->Config->OperTypes)
			ProcessMOTD(newmotds, type, "type");
		cmd.motds.swap(newmotds);

	}
};

MODULE_INIT(ModuleOperMOTD)
