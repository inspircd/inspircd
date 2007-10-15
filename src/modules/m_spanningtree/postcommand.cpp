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

void ModuleSpanningTree::OnPostCommand(const std::string &command, const char** parameters, int pcnt, User *user, CmdResult result, const std::string &original_line)
{
	if ((result == CMD_SUCCESS) && (ServerInstance->IsValidModuleCommand(command, pcnt, user)))
	{
		/* Safe, we know its non-null because IsValidModuleCommand returned true */
		Command* thiscmd = ServerInstance->Parser->GetHandler(command);
		// this bit of code cleverly routes all module commands
		// to all remote severs *automatically* so that modules
		// can just handle commands locally, without having
		// to have any special provision in place for remote
		// commands and linking protocols.
		std::deque<std::string> params;
		params.clear();
		int n_translate = thiscmd->translation.size();
		TranslateType translate_to;

		/* To make sure that parameters with spaces, or empty
		 * parameters, etc, are always sent properly, *always*
		 * prefix the last parameter with a :. This also removes
		 * an extra strchr() */
		for (int j = 0; j < pcnt; j++)
		{
			std::string target;

			/* Map all items to UUIDs where neccessary */
			if (j < n_translate)
			{
				/* We have a translation mapping for this index */
				translate_to = thiscmd->translation[j] != TR_END ? thiscmd->translation[j] : TR_TEXT;
			}
			else
				translate_to = TR_TEXT;

			ServerInstance->Log(DEBUG,"TRANSLATION: %s - type is %d", parameters[j], translate_to);
			ServerInstance->Parser->TranslateUIDs(translate_to, parameters[j], target);
			
			if (j == (pcnt - 1))
				params.push_back(":" + target);
			else
				params.push_back(target);
		}
		Utils->DoOneToMany(user->uuid, command, params);
	}
}

