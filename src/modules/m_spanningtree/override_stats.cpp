/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

ModResult ModuleSpanningTree::HandleStats(const std::vector<std::string>& parameters, User* user)
{
	if (parameters.size() > 1)
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, parameters[1]))
			return MOD_RES_PASSTHRU;

		/* Remote STATS, the server is within the 2nd parameter */
		parameterlist params;
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
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

ModResult ModuleSpanningTree::OnStats(char statschar, User* user, string_list &results)
{
	if ((statschar == 'c') || (statschar == 'n'))
	{
		for (unsigned int i = 0; i < Utils->LinkBlocks.size(); i++)
		{
			results.push_back(std::string(ServerInstance->Config->ServerName)+" 213 "+user->nick+" "+statschar+" *@"+(Utils->LinkBlocks[i]->HiddenFromStats ? "<hidden>" : Utils->LinkBlocks[i]->IPAddr)+" * "+Utils->LinkBlocks[i]->Name.c_str()+" "+ConvToStr(Utils->LinkBlocks[i]->Port)+" "+(Utils->LinkBlocks[i]->Hook.empty() ? "plaintext" : Utils->LinkBlocks[i]->Hook));
			if (statschar == 'c')
				results.push_back(std::string(ServerInstance->Config->ServerName)+" 244 "+user->nick+" H * * "+Utils->LinkBlocks[i]->Name.c_str());
		}
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

