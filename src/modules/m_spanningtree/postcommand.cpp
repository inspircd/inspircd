/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

void ModuleSpanningTree::OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, LocalUser *user, CmdResult result, const std::string &original_line)
{
	if (result == CMD_SUCCESS)
		Utils->RouteCommand(NULL, command, parameters, user);
}

void SpanningTreeUtilities::RouteCommand(TreeServer* origin, const std::string &command, const parameterlist& parameters, User *user)
{
	if (!ServerInstance->Parser->IsValidCommand(command, parameters.size(), user))
		return;

	/* We know it's non-null because IsValidCommand returned true */
	Command* thiscmd = ServerInstance->Parser->GetHandler(command);

	RouteDescriptor routing = thiscmd->GetRouting(user, parameters);

	std::string sent_cmd = command;
	parameterlist params;

	if (routing.type == ROUTE_TYPE_LOCALONLY)
	{
		/* Broadcast when it's a core command with the default route descriptor and the source is a
		 * remote user or a remote server
		 */

		Version ver = thiscmd->creator->GetVersion();
		if ((!(ver.Flags & VF_CORE)) || (IS_LOCAL(user)) || (IS_SERVER(user) == ServerInstance->FakeClient))
			return;

		routing = ROUTE_BROADCAST;
	}
	else if (routing.type == ROUTE_TYPE_OPT_BCAST)
	{
		params.push_back("*");
		params.push_back(command);
		sent_cmd = "ENCAP";
	}
	else if (routing.type == ROUTE_TYPE_OPT_UCAST)
	{
		TreeServer* sdest = FindServer(routing.serverdest);
		if (!sdest)
		{
			ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Trying to route ENCAP to nonexistent server %s",
				routing.serverdest.c_str());
			return;
		}
		params.push_back(sdest->GetID());
		params.push_back(command);
		sent_cmd = "ENCAP";
	}
	else
	{
		Module* srcmodule = thiscmd->creator;
		Version ver = srcmodule->GetVersion();

		if (!(ver.Flags & (VF_COMMON | VF_CORE)) && srcmodule != Creator)
		{
			ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Routed command %s from non-VF_COMMON module %s",
				command.c_str(), srcmodule->ModuleSourceFile.c_str());
			return;
		}
	}

	std::string output_text;
	ServerInstance->Parser->TranslateUIDs(thiscmd->translation, parameters, output_text, true, thiscmd);

	params.push_back(output_text);

	if (routing.type == ROUTE_TYPE_MESSAGE)
	{
		char pfx = 0;
		std::string dest = routing.serverdest;
		if (ServerInstance->Modes->FindPrefix(dest[0]))
		{
			pfx = dest[0];
			dest = dest.substr(1);
		}
		if (dest[0] == '#')
		{
			Channel* c = ServerInstance->FindChan(dest);
			if (!c)
				return;
			TreeServerList list;
			// TODO OnBuildExemptList hook was here
			GetListOfServersForChannel(c,list,pfx, CUList());
			std::string data = ":" + user->uuid + " " + sent_cmd;
			for (unsigned int x = 0; x < params.size(); x++)
				data += " " + params[x];
			for (TreeServerList::iterator i = list.begin(); i != list.end(); i++)
			{
				TreeSocket* Sock = i->second->GetSocket();
				if (origin && origin->GetSocket() == Sock)
					continue;
				if (Sock)
					Sock->WriteLine(data);
			}
		}
		else if (dest[0] == '$')
		{
			if (origin)
				DoOneToAllButSender(user->uuid, sent_cmd, params, origin->GetName());
			else
				DoOneToMany(user->uuid, sent_cmd, params);
		}
		else
		{
			// user target?
			User* d = ServerInstance->FindNick(dest);
			if (!d)
				return;
			TreeServer* tsd = BestRouteTo(d->server);
			if (tsd == origin)
				// huh? no routing stuff around in a circle, please.
				return;
			DoOneToOne(user->uuid, sent_cmd, params, d->server);
		}
	}
	else if (routing.type == ROUTE_TYPE_BROADCAST || routing.type == ROUTE_TYPE_OPT_BCAST)
	{
		if (origin)
			DoOneToAllButSender(user->uuid, sent_cmd, params, origin->GetName());
		else
			DoOneToMany(user->uuid, sent_cmd, params);
	}
	else if (routing.type == ROUTE_TYPE_UNICAST || routing.type == ROUTE_TYPE_OPT_UCAST)
	{
		if (origin && routing.serverdest == origin->GetName())
			return;
		DoOneToOne(user->uuid, sent_cmd, params, routing.serverdest);
	}
}
