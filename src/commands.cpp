/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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

CmdResult SplitCommand::Handle(User* user, const Params& parameters)
{
	switch (user->usertype)
	{
		case USERTYPE_LOCAL:
			return HandleLocal(static_cast<LocalUser*>(user), parameters);

		case USERTYPE_REMOTE:
			return HandleRemote(static_cast<RemoteUser*>(user), parameters);

		case USERTYPE_SERVER:
			return HandleServer(static_cast<FakeUser*>(user), parameters);
	}

	ServerInstance->Logs->Log("COMMAND", LOG_DEFAULT, "Unknown user type %d in command (uuid=%s)!",
		user->usertype, user->uuid.c_str());
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleLocal(LocalUser* user, const Params& parameters)
{
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleRemote(RemoteUser* user, const Params& parameters)
{
	return CMD_INVALID;
}

CmdResult SplitCommand::HandleServer(FakeUser* user, const Params& parameters)
{
	return CMD_INVALID;
}
