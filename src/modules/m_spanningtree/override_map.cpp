/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "commands.h"

CommandMap::CommandMap(Module* Creator)
	: Command(Creator, "MAP", 0, 1)
{
	Penalty = 2;
}

static inline bool IsHidden(User* user, TreeServer* server)
{
	if (!user->IsOper())
	{
		if (server->Hidden)
			return true;
		if (Utils->HideULines && server->IsULine())
			return true;
	}

	return false;
}

// Calculate the map depth the servers go, and the longest server name
static void GetDepthAndLen(TreeServer* current, unsigned int depth, unsigned int& max_depth, unsigned int& max_len)
{
	if (depth > max_depth)
		max_depth = depth;
	if (current->GetName().length() > max_len)
		max_len = current->GetName().length();

	const TreeServer::ChildServers& servers = current->GetChildren();
	for (TreeServer::ChildServers::const_iterator i = servers.begin(); i != servers.end(); ++i)
	{
		TreeServer* child = *i;
		GetDepthAndLen(child, depth + 1, max_depth, max_len);
	}
}

static std::vector<std::string> GetMap(User* user, TreeServer* current, unsigned int max_len, unsigned int depth)
{
	float percent = 0;

	const user_hash& users = ServerInstance->Users->GetUsers();
	if (!users.empty())
	{
		// If there are no users, WHO THE HELL DID THE /MAP?!?!?!
		percent = current->UserCount * 100.0 / users.size();
	}

	std::string buffer = current->GetName();
	if (user->IsOper())
	{
		buffer += " (" + current->GetID() + ")";
	}

	// Pad with spaces until its at max len, max_len must always be >= my names length
	buffer.append(max_len - current->GetName().length(), ' ');

	buffer += InspIRCd::Format("%5d [%5.2f%%]", current->UserCount, percent);

	if (user->IsOper())
	{
		time_t secs_up = ServerInstance->Time() - current->age;
		buffer += " [Up: " + ModuleSpanningTree::TimeToStr(secs_up) + (current->rtt == 0 ? "]" : " Lag: " + ConvToStr(current->rtt) + "ms]");
	}

	std::vector<std::string> map;
	map.push_back(buffer);

	const TreeServer::ChildServers& servers = current->GetChildren();
	for (TreeServer::ChildServers::const_iterator i = servers.begin(); i != servers.end(); ++i)
	{
		TreeServer* child = *i;

		if (IsHidden(user, child))
			continue;

		bool last = true;
		for (TreeServer::ChildServers::const_iterator j = i + 1; last && j != servers.end(); ++j)
			if (!IsHidden(user, *j))
				last = false;

		unsigned int next_len;

		if (user->IsOper() || !Utils->FlatLinks)
		{
			// This child is indented by us, so remove the depth from the max length to align the users properly
			next_len = max_len - 2;
		}
		else
		{
			// This user can not see depth, so max_len remains constant
			next_len = max_len;
		}

		// Build the map for this child
		std::vector<std::string> child_map = GetMap(user, child, next_len, depth + 1);

		for (std::vector<std::string>::const_iterator j = child_map.begin(); j != child_map.end(); ++j)
		{
			const char* prefix;

			if (user->IsOper() || !Utils->FlatLinks)
			{
				// If this server is not the root child
				if (j != child_map.begin())
				{
					// If this child is not my last child, then add |
					// to be able to "link" the next server in my list to me, and to indent this childs servers
					if (!last)
						prefix = "| ";
					// Otherwise this is my last child, so just use a space as theres nothing else linked to me below this
					else
						prefix = "  ";
				}
				// If we get here, this server must be the root child
				else
				{
					// If this is the last child, it gets a `-
					if (last)
						prefix = "`-";
					// Otherwise this isn't the last child, so it gets |-
					else
						prefix = "|-";
				}
			}
			else
				// User can't see depth, so use no prefix
				prefix = "";

			// Add line to the map
			map.push_back(prefix + *j);
		}
	}

	return map;
}

CmdResult CommandMap::Handle(const std::vector<std::string>& parameters, User* user)
{
	if (parameters.size() > 0)
	{
		// Remote MAP, the target server is the 1st parameter
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		if (!s)
		{
			user->WriteNumeric(ERR_NOSUCHSERVER, parameters[0], "No such server");
			return CMD_FAILURE;
		}

		if (!s->IsRoot())
			return CMD_SUCCESS;
	}

	// Max depth and max server name length
	unsigned int max_depth = 0;
	unsigned int max_len = 0;
	GetDepthAndLen(Utils->TreeRoot, 0, max_depth, max_len);

	unsigned int max;
	if (user->IsOper() || !Utils->FlatLinks)
	{
		// Each level of the map is indented by 2 characters, making the max possible line (max_depth * 2) + max_len
		max = (max_depth * 2) + max_len;
	}
	else
	{
		// This user can't see any depth
		max = max_len;
	}

	std::vector<std::string> map = GetMap(user, Utils->TreeRoot, max, 0);
	for (std::vector<std::string>::const_iterator i = map.begin(); i != map.end(); ++i)
		user->WriteRemoteNumeric(RPL_MAP, *i);

	size_t totusers = ServerInstance->Users->GetUsers().size();
	float avg_users = (float) totusers / Utils->serverlist.size();

	user->WriteRemoteNumeric(RPL_MAPUSERS, InspIRCd::Format("%u server%s and %u user%s, average %.2f users per server",
		(unsigned int)Utils->serverlist.size(), (Utils->serverlist.size() > 1 ? "s" : ""), (unsigned int)totusers, (totusers > 1 ? "s" : ""), avg_users));
	user->WriteRemoteNumeric(RPL_ENDMAP, "End of /MAP");

	return CMD_SUCCESS;
}

RouteDescriptor CommandMap::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	if (!parameters.empty())
		return ROUTE_UNICAST(parameters[0]);
	return ROUTE_LOCALONLY;
}
