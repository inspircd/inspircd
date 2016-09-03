/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

CommandVersion::CommandVersion(Module* parent)
	: Command(parent, "VERSION", 0, 0)
{
	syntax = "[<servername>]";
}

CmdResult CommandVersion::Handle (const std::vector<std::string>&, User *user)
{
	std::string version = ServerInstance->GetVersionString((user->IsOper()));
	user->WriteNumeric(RPL_VERSION, version);
	LocalUser *lu = IS_LOCAL(user);
	if (lu != NULL)
	{
		ServerInstance->ISupport.SendTo(lu);
	}
	return CMD_SUCCESS;
}
