/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */
		
#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"      
#include "transport.h"  
			
#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rconnect.h"
#include "m_spanningtree/rsquit.h"
	
/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rconnect.h m_spanningtree/rsquit.h */

int ModuleSpanningTree::HandleStats(const char** parameters, int pcnt, User* user)
{
	if (pcnt > 1)
	{
		if (match(ServerInstance->Config->ServerName, parameters[1]))
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
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[1]);
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
		results.push_back(std::string(ServerInstance->Config->ServerName)+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
		ServerInstance->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",(!strcmp(user->server,ServerInstance->Config->ServerName) ? "Stats" : "Remote stats"),statschar,user->nick,user->ident,user->host);
		return 1;
	}

	if (statschar == 'p')
	{
		/* show all server ports, after showing client ports. -- w00t */

		for (unsigned int i = 0; i < Utils->Bindings.size(); i++)
		{
			std::string ip = Utils->Bindings[i]->IP;
			if (ip.empty())
				ip = "*";

			std::string transport("plaintext");
			if (Utils->Bindings[i]->GetHook())
				transport = BufferedSocketNameRequest(this, Utils->Bindings[i]->GetHook()).Send();

			results.push_back(ConvToStr(ServerInstance->Config->ServerName) + " 249 "+user->nick+" :" + ip + ":" + ConvToStr(Utils->Bindings[i]->port)+
				" (server, " + transport + ")");
		}
	}
	return 0;
}

