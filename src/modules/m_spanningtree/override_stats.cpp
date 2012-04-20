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


/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

int ModuleSpanningTree::HandleStats(const std::vector<std::string>& parameters, User* user)
{
	if (parameters.size() > 1)
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, parameters[1]))
			return 0;

		/* Remote STATS, the server is within the 2nd parameter */
		std::deque<std::string> params;
		params.push_back(parameters[0]);
		params.push_back(parameters[1]);
		/* Send it out remotely, generate no reply yet */

		TreeServer* s = Utils->FindServerMask(parameters[1]);
		if (s)
		{
			params[1] = s->GetName();
			Utils->DoOneToOne(user->uuid, "STATS", params, s->GetName());
		}
		else
		{
			user->WriteServ( "402 %s %s :No such server", user->nick.c_str(), parameters[1].c_str());
		}
		return 1;
	}
	return 0;
}

int ModuleSpanningTree::OnStats(char statschar, User* user, string_list &results)
{
	if ((statschar == 'c') || (statschar == 'n'))
	{
		for (unsigned int i = 0; i < Utils->LinkBlocks.size(); i++)
		{
			results.push_back(std::string(ServerInstance->Config->ServerName)+" 213 "+user->nick+" "+statschar+" *@"+(Utils->LinkBlocks[i].HiddenFromStats ? "<hidden>" : Utils->LinkBlocks[i].IPAddr)+" * "+Utils->LinkBlocks[i].Name.c_str()+" "+ConvToStr(Utils->LinkBlocks[i].Port)+" "+(Utils->LinkBlocks[i].Hook.empty() ? "plaintext" : Utils->LinkBlocks[i].Hook)+" "+(Utils->LinkBlocks[i].AutoConnect ? 'a': '-')+'s');
			if (statschar == 'c')
				results.push_back(std::string(ServerInstance->Config->ServerName)+" 244 "+user->nick+" H * * "+Utils->LinkBlocks[i].Name.c_str());
		}
		return 1;
	}

	if (statschar == 'p')
	{
		/* show all server ports, after showing client ports. -- w00t */

		for (unsigned int i = 0; i < Utils->Bindings.size(); i++)
		{
			std::string ip = Utils->Bindings[i]->GetIP();
			if (ip.empty())
				ip = "*";

			std::string transport("plaintext");
			if (Utils->Bindings[i]->GetIOHook())
				transport = BufferedSocketNameRequest(this, Utils->Bindings[i]->GetIOHook()).Send();

			results.push_back(ConvToStr(ServerInstance->Config->ServerName) + " 249 "+user->nick+" :" + ip + ":" + ConvToStr(Utils->Bindings[i]->GetPort())+
				" (server, " + transport + ")");
		}
	}
	return 0;
}

