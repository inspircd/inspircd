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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rsquit.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h m_spanningtree/rsquit.h */

CommandRSQuit::CommandRSQuit (InspIRCd* Instance, Module* Callback, SpanningTreeUtilities* Util) : Command(Instance, "RSQUIT", "o", 1), Creator(Callback), Utils(Util)
{
	this->source = "m_spanningtree.so";
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
			sock->Squit(server_target, std::string("Server quit by ") + user->GetFullRealHost() + " (" + reason + ")");
			ServerInstance->SE->DelFd(sock);
			sock->Close();
			return CMD_LOCALONLY;
		}
	}

	return CMD_SUCCESS;
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
		std::deque<std::string> params;
		params.push_back(user->nick);
		params.push_back("NOTICE "+ConvToStr(user->nick)+" :"+msg);
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", params, user->server);
	}
}

