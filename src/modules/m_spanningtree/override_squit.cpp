/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

ModResult ModuleSpanningTree::HandleSquit(const std::vector<std::string>& parameters, User* user)
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
			ServerInstance->SNO->WriteToSnoMask('l',"SQUIT: Server \002%s\002 removed from network by %s",parameters[0].c_str(),user->nick.c_str());
			s->SQuit("Server quit by " + user->GetFullRealHost());
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
