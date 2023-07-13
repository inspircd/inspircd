/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

#include "core_oper.h"

CommandDie::CommandDie(Module* parent)
	: Command(parent, "DIE", 1, 1)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<servername>" };
}

void DieRestart::SendError(const std::string& message)
{
	ClientProtocol::Messages::Error errormsg(message);
	ClientProtocol::Event errorevent(ServerInstance->GetRFCEvents().error, errormsg);

	for (auto* user : ServerInstance->Users.GetLocalUsers())
	{
		if (user->IsFullyConnected())
		{
			user->WriteNotice(message);
		}
		else
		{
			// Partially connected users receive ERROR, not a NOTICE
			user->Send(errorevent);
		}
	}
}

CmdResult CommandDie::Handle(User* user, const Params& parameters)
{
	if (irc::equals(parameters[0], ServerInstance->Config->ServerName))
	{
		const std::string diebuf = "*** DIE command from " + user->GetMask() + ". Terminating.";
		ServerInstance->Logs.Error(MODNAME, diebuf);
		DieRestart::SendError(diebuf);
		ServerInstance->Exit(EXIT_FAILURE);
	}
	else
	{
		ServerInstance->Logs.Error(MODNAME, "Failed /DIE command from {}", user->GetRealMask());
		ServerInstance->SNO.WriteGlobalSno('a', "Failed DIE command from {}.", user->GetRealMask());
		return CmdResult::FAILURE;
	}
}
