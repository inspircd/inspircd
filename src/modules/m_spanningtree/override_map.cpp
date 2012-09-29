/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

const std::string ModuleSpanningTree::MapOperInfo(TreeServer* Current)
{
	time_t secs_up = ServerInstance->Time() - Current->age;
	return " [Up: " + TimeToStr(secs_up) + (Current->rtt == 0 ? "]" : " Lag: " + ConvToStr(Current->rtt) + "ms]");
}

void ModuleSpanningTree::ShowMap(TreeServer* Current, User* user, int depth, int &line, char* names, int &maxnamew, char* stats)
{
	ServerInstance->Logs->Log("map",DEBUG,"ShowMap depth %d on line %d", depth, line);
	float percent;

	if (ServerInstance->Users->clientlist->size() == 0)
	{
		// If there are no users, WHO THE HELL DID THE /MAP?!?!?!
		percent = 0;
	}
	else
	{
		percent = Current->GetUserCount() * 100.0 / ServerInstance->Users->clientlist->size();
	}

	const std::string operdata = IS_OPER(user) ? MapOperInfo(Current) : "";

	char* myname = names + 100 * line;
	char* mystat = stats + 50 * line;
	memset(myname, ' ', depth);
	int w = depth;

	std::string servername = Current->GetName();
	if (IS_OPER(user))
	{
		w += snprintf(myname + depth, 99 - depth, "%s (%s)", servername.c_str(), Current->GetID().c_str());
	}
	else
	{
		w += snprintf(myname + depth, 99 - depth, "%s", servername.c_str());
	}
	memset(myname + w, ' ', 100 - w);
	if (w > maxnamew)
		maxnamew = w;
	snprintf(mystat, 49, "%5d [%5.2f%%]%s", Current->GetUserCount(), percent, operdata.c_str());

	line++;

	if (IS_OPER(user) || !Utils->FlatLinks)
		depth = depth + 2;
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* child = Current->GetChild(q);
		if (!IS_OPER(user)) {
			if (child->Hidden)
				continue;
			if ((Utils->HideULines) && (ServerInstance->ULine(child->GetName())))
				continue;
		}
		ShowMap(child, user, depth, line, names, maxnamew, stats);
	}
}


// Ok, prepare to be confused.
// After much mulling over how to approach this, it struck me that
// the 'usual' way of doing a /MAP isnt the best way. Instead of
// keeping track of a ton of ascii characters, and line by line
// under recursion working out where to place them using multiplications
// and divisons, we instead render the map onto a backplane of characters
// (a character matrix), then draw the branches as a series of "L" shapes
// from the nodes. This is not only friendlier on CPU it uses less stack.
bool ModuleSpanningTree::HandleMap(const std::vector<std::string>& parameters, User* user)
{
	if (parameters.size() > 0)
	{
		/* Remote MAP, the server is within the 1st parameter */
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		bool ret = false;
		if (!s)
		{
			user->WriteNumeric(ERR_NOSUCHSERVER, "%s %s :No such server", user->nick.c_str(), parameters[0].c_str());
			ret = true;
		}
		else if (s && s != Utils->TreeRoot)
		{
			parameterlist params;
			params.push_back(parameters[0]);

			params[0] = s->GetName();
			Utils->DoOneToOne(user->uuid, "MAP", params, s->GetName());
			ret = true;
		}

		// Don't return if s == Utils->TreeRoot (us)
		if (ret)
			return true;
	}

	// These arrays represent a virtual screen which we will
	// "scratch" draw to, as the console device of an irc
	// client does not provide for a proper terminal.
	int totusers = ServerInstance->Users->clientlist->size();
	int totservers = this->CountServs();
	int maxnamew = 0;
	int line = 0;
	char* names = new char[totservers * 100];
	char* stats = new char[totservers * 50];

	// The only recursive bit is called here.
	ShowMap(Utils->TreeRoot,user,0,line,names,maxnamew,stats);

	// Process each line one by one.
	for (int l = 1; l < line; l++)
	{
		char* myname = names + 100 * l;
		// scan across the line looking for the start of the
		// servername (the recursive part of the algorithm has placed
		// the servers at indented positions depending on what they
		// are related to)
		int first_nonspace = 0;

		while (myname[first_nonspace] == ' ')
		{
			first_nonspace++;
		}

		first_nonspace--;

		// Draw the `- (corner) section: this may be overwritten by
		// another L shape passing along the same vertical pane, becoming
		// a |- (branch) section instead.

		myname[first_nonspace] = '-';
		myname[first_nonspace-1] = '`';
		int l2 = l - 1;

		// Draw upwards until we hit the parent server, causing possibly
		// other corners (`-) to become branches (|-)
		while ((names[l2 * 100 + first_nonspace-1] == ' ') || (names[l2 * 100 + first_nonspace-1] == '`'))
		{
			names[l2 * 100 + first_nonspace-1] = '|';
			l2--;
		}
	}

	float avg_users = totusers * 1.0 / line;

	ServerInstance->Logs->Log("map",DEBUG,"local");
	for (int t = 0; t < line; t++)
	{
		// terminate the string at maxnamew characters
		names[100 * t + maxnamew] = '\0';
		user->SendText(":%s %03d %s :%s %s", ServerInstance->Config->ServerName.c_str(),
			RPL_MAP, user->nick.c_str(), names + 100 * t, stats + 50 * t);
	}
	user->SendText(":%s %03d %s :%d server%s and %d user%s, average %.2f users per server",
		ServerInstance->Config->ServerName.c_str(), RPL_MAPUSERS, user->nick.c_str(),
		line, (line > 1 ? "s" : ""), totusers, (totusers > 1 ? "s" : ""), avg_users);
	user->SendText(":%s %03d %s :End of /MAP", ServerInstance->Config->ServerName.c_str(),
		RPL_ENDMAP, user->nick.c_str());

	delete[] names;
	delete[] stats;

	return true;
}

