/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

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

