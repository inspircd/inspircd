/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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
#include "xline.h"
#include "../transport.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

void ModuleSpanningTree::OnPostCommand(const std::string &command, const std::vector<std::string>& parameters, User *user, CmdResult result, const std::string &original_line)
{
	if ((result == CMD_SUCCESS) && (ServerInstance->IsValidModuleCommand(command, parameters.size(), user)))
	{
		/* Safe, we know its non-null because IsValidModuleCommand returned true */
		Command* thiscmd = ServerInstance->Parser->GetHandler(command);

		Module* srcmodule = ServerInstance->Modules->Find(thiscmd->source);

		if (srcmodule && !(srcmodule->GetVersion().Flags & VF_COMMON)) {
			ServerInstance->Logs->Log("m_spanningtree",ERROR,"Routed command %s from non-VF_COMMON module %s",
				command.c_str(), thiscmd->source.c_str());
			return;
		}

		// this bit of code cleverly routes all module commands
		// to all remote severs *automatically* so that modules
		// can just handle commands locally, without having
		// to have any special provision in place for remote
		// commands and linking protocols.
		std::deque<std::string> params;
		params.clear();
		unsigned int n_translate = thiscmd->translation.size();
		TranslateType translate_to;

		/* To make sure that parameters with spaces, or empty
		 * parameters, etc, are always sent properly, *always*
		 * prefix the last parameter with a :. This also removes
		 * an extra strchr() */
		for (unsigned int j = 0; j < parameters.size(); j++)
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

			ServerInstance->Logs->Log("m_spanningtree",DEBUG,"TRANSLATION: %s - type is %d", parameters[j].c_str(), translate_to);
			if (translate_to == TR_CUSTOM)
			{
				target = parameters[j];
				thiscmd->EncodeParameter(target, j);
			}
			else
			{
				ServerInstance->Parser->TranslateUIDs(translate_to, parameters[j], target);
			}

			if (j == (parameters.size() - 1))
				params.push_back(":" + target);
			else
				params.push_back(target);
		}
		Utils->DoOneToMany(user->uuid, command, params);
	}
}

