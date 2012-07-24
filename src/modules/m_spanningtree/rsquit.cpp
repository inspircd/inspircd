/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "socket.h"
#include "xline.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"
#include "commands.h"

CommandRSQuit::CommandRSQuit (Module* Creator, SpanningTreeUtilities* Util)
	: Command(Creator, "RSQUIT", 1), Utils(Util)
{
	flags_needed = 'o';
	syntax = "<target-server-mask> [reason]";
}

CmdResult CommandRSQuit::Handle (const std::vector<std::string>& parameters, User *user)
{
	TreeServer *server_target; // Server to squit
	TreeServer *server_linked; // Server target is linked to

	server_target = Utils->FindServerMask(parameters[0]);
	if (!server_target)
	{
		user->WriteServ("NOTICE %s :*** RSQUIT: Server \002%s\002 isn't connected to the network!", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (server_target == Utils->TreeRoot)
	{
		NoticeUser(user, "*** RSQUIT: Foolish mortal, you cannot make a server SQUIT itself! ("+parameters[0]+" matches local server name)");
		return CMD_FAILURE;
	}

	server_linked = server_target->GetParent();

	if (server_linked == Utils->TreeRoot)
	{
		// We have been asked to remove server_target.
		TreeSocket* sock = server_target->GetSocket();
		if (sock)
		{
			const char *reason = parameters.size() == 2 ? parameters[1].c_str() : "No reason";
			ServerInstance->SNO->WriteToSnoMask('l',"RSQUIT: Server \002%s\002 removed from network by %s (%s)", parameters[0].c_str(), user->nick.c_str(), reason);
			sock->Squit(server_target, "Server quit by " + user->GetFullRealHost() + " (" + reason + ")");
			sock->Close();
		}
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandRSQuit::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return ROUTE_UNICAST(parameters[0]);
}

// XXX use protocol interface instead of rolling our own :)
void CommandRSQuit::NoticeUser(User* user, const std::string &msg)
{
	if (IS_LOCAL(user))
	{
		user->WriteServ("NOTICE %s :%s",user->nick.c_str(),msg.c_str());
	}
	else
	{
		parameterlist params;
		params.push_back(user->nick);
		params.push_back("NOTICE "+ConvToStr(user->nick)+" :"+msg);
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", params, user->server);
	}
}

