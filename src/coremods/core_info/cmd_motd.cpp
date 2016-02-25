/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "core_info.h"

CommandMotd::CommandMotd(Module* parent)
	: Command(parent, "MOTD", 0, 1)
{
	syntax = "[<servername>]";
}

/** Handle /MOTD
 */
CmdResult CommandMotd::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
	{
		// Give extra penalty if a non-oper queries the /MOTD of a remote server
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (!user->IsOper()))
			localuser->CommandFloodPenalty += 2000;
		return CMD_SUCCESS;
	}

	ConfigTag* tag = ServerInstance->Config->EmptyTag;
	LocalUser* localuser = IS_LOCAL(user);
	if (localuser)
		tag = localuser->GetClass()->config;
	std::string motd_name = tag->getString("motd", "motd");
	ConfigFileCache::iterator motd = ServerInstance->Config->Files.find(motd_name);
	if (motd == ServerInstance->Config->Files.end())
	{
		user->WriteRemoteNumeric(ERR_NOMOTD, "Message of the day file is missing.");
		return CMD_SUCCESS;
	}

	user->WriteRemoteNumeric(RPL_MOTDSTART, InspIRCd::Format("%s message of the day", ServerInstance->Config->ServerName.c_str()));

	for (file_cache::iterator i = motd->second.begin(); i != motd->second.end(); i++)
		user->WriteRemoteNumeric(RPL_MOTD, InspIRCd::Format("- %s", i->c_str()));

	user->WriteRemoteNumeric(RPL_ENDOFMOTD, "End of message of the day.");

	return CMD_SUCCESS;
}

RouteDescriptor CommandMotd::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	if (parameters.size() > 0)
		return ROUTE_UNICAST(parameters[0]);
	return ROUTE_LOCALONLY;
}
