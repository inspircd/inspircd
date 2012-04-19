/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#ifndef WIN32
	#include <sys/resource.h>
	/* This is just to be completely certain that the change which fixed getrusage on RH7 doesn't break anything else -- Om */
	#ifndef RUSAGE_SELF
	#define RUSAGE_SELF 0
	#endif
#else
	#include <psapi.h>
	#include "inspircd_win32wrapper.h"
	#pragma comment(lib, "psapi.lib")
#endif

#include "xline.h"

/** Handle /STATS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandStats : public Command
{
 public:
	/** Constructor for stats.
	 */
	CommandStats ( Module* parent) : Command(parent,"STATS",1,2) { syntax = "<stats-symbol> [<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 1)
			return ROUTE_UNICAST(parameters[1]);
		return ROUTE_LOCALONLY;
	}
};

CmdResult CommandStats::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 1 && parameters[1] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;
	string_list values;
	char search = parameters[0][0];
	ServerInstance->DoStats(search, user, values);
	for (size_t i = 0; i < values.size(); i++)
		user->SendText(":%s", values[i].c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandStats)
