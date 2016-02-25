/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2004 Christopher Hall <typobox43@gmail.com>
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

/** Handle /OPERMOTD
 */
class CommandOpermotd : public Command
{
 public:
	file_cache opermotd;

	CommandOpermotd(Module* Creator) : Command(Creator,"OPERMOTD", 0, 1)
	{
		flags_needed = 'o'; syntax = "[<servername>]";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User* user)
	{
		if ((parameters.empty()) || (parameters[0] == ServerInstance->Config->ServerName))
			ShowOperMOTD(user);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (!parameters.empty())
			return ROUTE_OPT_UCAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}

	void ShowOperMOTD(User* user)
	{
		if (opermotd.empty())
		{
			user->WriteRemoteNumeric(455, "OPERMOTD file is missing");
			return;
		}

		user->WriteRemoteNumeric(375, "- IRC Operators Message of the Day");

		for (file_cache::const_iterator i = opermotd.begin(); i != opermotd.end(); ++i)
		{
			user->WriteRemoteNumeric(372, InspIRCd::Format("- %s", i->c_str()));
		}

		user->WriteRemoteNumeric(376, "- End of OPERMOTD");
	}
};


class ModuleOpermotd : public Module
{
	CommandOpermotd cmd;
	bool onoper;
 public:

	ModuleOpermotd()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Shows a message to opers after oper-up, adds /opermotd", VF_VENDOR | VF_OPTCOMMON);
	}

	void OnOper(User* user, const std::string &opertype) CXX11_OVERRIDE
	{
		if (onoper && IS_LOCAL(user))
			cmd.ShowOperMOTD(user);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		cmd.opermotd.clear();
		ConfigTag* conf = ServerInstance->Config->ConfValue("opermotd");
		onoper = conf->getBool("onoper", true);

		try
		{
			FileReader reader(conf->getString("file", "opermotd"));
			cmd.opermotd = reader.GetVector();
		}
		catch (CoreException&)
		{
			// Nothing happens here as we do the error handling in ShowOperMOTD.
		}

		if (conf->getBool("processcolors"))
			InspIRCd::ProcessColors(cmd.opermotd);
	}
};

MODULE_INIT(ModuleOpermotd)
