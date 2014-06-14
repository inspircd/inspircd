/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
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

CmdResult SplitCommand::Handle(const std::vector<std::string>& parms, User* u)
{
	if (IS_LOCAL(u))
		return HandleLocal(parms, IS_LOCAL(u));
	if (IS_REMOTE(u))
		return HandleRemote(parms, IS_REMOTE(u));
	if (IS_SERVER(u))
		return HandleServer(parms, IS_SERVER(u));
	ServerInstance->Logs->Log("COMMAND", LOG_DEFAULT, "Unknown user type in command (uuid=%s)!", u->uuid.c_str());
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleLocal(const std::vector<std::string>&, LocalUser*)
{
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleRemote(const std::vector<std::string>&, RemoteUser*)
{
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleServer(const std::vector<std::string>&, FakeUser*)
{
	return CMD_INVALID;
}
