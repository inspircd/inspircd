/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "socket.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

ModResult ModuleSpanningTree::HandleSquit(const CommandBase::Params& parameters, User* user)
{
	TreeServer* s = Utils->FindServerMask(parameters[0]);
	if (s)
	{
		if (s->IsRoot())
		{
			user->WriteNotice("*** SQUIT: Foolish mortal, you cannot make a server SQUIT itself! (" + parameters[0] + " matches local server name)");
			return MOD_RES_DENY;
		}

		if (s->IsLocal())
		{
			ServerInstance->SNO.WriteToSnoMask('l', "SQUIT: Server \002{}\002 removed from network by {}", parameters[0], user->nick);
			s->SQuit("Server quit by " + user->GetRealMask());
		}
		else
		{
			user->WriteNotice("*** SQUIT may not be used to remove remote servers. Please use RSQUIT instead.");
		}
	}
	else
	{
		user->WriteNotice("*** SQUIT: The server \002" + parameters[0] + "\002 does not exist on the network.");
	}
	return MOD_RES_DENY;
}
