/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
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

class CommandOpermotd final
	: public Command
{
public:
	std::vector<std::string> opermotd;

	CommandOpermotd(Module* Creator)
		: Command(Creator, "OPERMOTD")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<servername>]" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if ((parameters.empty()) || (irc::equals(parameters[0], ServerInstance->Config->ServerName)))
			ShowOperMOTD(user, true);
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		if ((!parameters.empty()) && (parameters[0].find('.') != std::string::npos))
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}

	void ShowOperMOTD(User* user, bool show_missing)
	{
		if (opermotd.empty())
		{
			if (show_missing)
				user->WriteRemoteNumeric(ERR_NOOPERMOTD, "OPERMOTD file is missing.");
			return;
		}

		user->WriteRemoteNumeric(RPL_OMOTDSTART, "Server operators message of the day");
		for (const auto& line : opermotd)
			user->WriteRemoteNumeric(RPL_OMOTD, InspIRCd::Format(" %s", line.c_str()));
		user->WriteRemoteNumeric(RPL_ENDOFOMOTD, "End of OPERMOTD");
	}
};

class ModuleOpermotd final
	: public Module
{
private:
	CommandOpermotd cmd;
	bool onoper;

public:
	ModuleOpermotd()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /OPERMOTD command which adds a special message of the day for server operators.")
		, cmd(this)
	{
	}

	void OnPostOperLogin(User* user) override
	{
		if (onoper && IS_LOCAL(user))
			cmd.ShowOperMOTD(user, false);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		cmd.opermotd.clear();
		auto conf = ServerInstance->Config->ConfValue("opermotd");
		onoper = conf->getBool("onoper", true);

		try
		{
			FileReader reader(conf->getString("file", "opermotd", 1));
			cmd.opermotd = reader.GetVector();
			InspIRCd::ProcessColors(cmd.opermotd);
		}
		catch (const CoreException&)
		{
			// Nothing happens here as we do the error handling in ShowOperMOTD.
		}
	}
};

MODULE_INIT(ModuleOpermotd)
