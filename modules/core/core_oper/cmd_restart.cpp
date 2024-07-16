/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Jordyn/The Linux Geek <onlinecloud1@gmail.com>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#ifdef _WIN32
# include <process.h>
#else
# include <fcntl.h>
# include <unistd.h>
#endif

#include "inspircd.h"
#include "core_oper.h"

CommandRestart::CommandRestart(Module* parent)
	: Command(parent, "RESTART", 1, 1)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<servername>" };
}

CmdResult CommandRestart::Handle(User* user, const Params& parameters)
{
	ServerInstance->Logs.Normal(MODNAME, "Restart: {}", user->nick);
	if (irc::equals(parameters[0], ServerInstance->Config->ServerName))
	{
		ServerInstance->SNO.WriteGlobalSno('a', "RESTART command from {}, restarting server.", user->GetRealMask());

		DieRestart::SendError("Server restarting.");

#ifdef FD_CLOEXEC
		/* XXX: This hack sets FD_CLOEXEC on all possible file descriptors, so they're closed if the execvp() below succeeds.
		 * Certainly, this is not a nice way to do things and it's slow when the fd limit is high.
		 *
		 * A better solution would be to set the close-on-exec flag for each fd we create (or create them with O_CLOEXEC),
		 * however there is no guarantee that third party libs will do the same.
		 */
		for (int i = (int)SocketEngine::GetMaxFds(); --i > 2; )
		{
			int flags = fcntl(i, F_GETFD);
			if (flags != -1)
				fcntl(i, F_SETFD, flags | FD_CLOEXEC);
		}
#endif

		execvp(ServerInstance->Config->CommandLine.argv[0], ServerInstance->Config->CommandLine.argv);
		ServerInstance->SNO.WriteGlobalSno('a', "Failed RESTART - could not execute '{}' ({})",
			ServerInstance->Config->CommandLine.argv[0], strerror(errno));
	}
	else
	{
		ServerInstance->SNO.WriteGlobalSno('a', "Failed RESTART Command from {}.", user->GetRealMask());
	}
	return CmdResult::FAILURE;
}
