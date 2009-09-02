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
	if (result != CMD_SUCCESS)
		return;
	if (!ServerInstance->IsValidModuleCommand(command, parameters.size(), user))
		return;

	/* We know it's non-null because IsValidModuleCommand returned true */
	Command* thiscmd = ServerInstance->Parser->GetHandler(command);

	RouteDescriptor routing = thiscmd->GetRouting(user, parameters);

	if (routing.type == ROUTE_TYPE_LOCALONLY)
		return;

	Module* srcmodule = ServerInstance->Modules->Find(thiscmd->source);

	if (srcmodule && !(srcmodule->GetVersion().Flags & VF_COMMON)) {
		ServerInstance->Logs->Log("m_spanningtree",ERROR,"Routed command %s from non-VF_COMMON module %s",
			command.c_str(), thiscmd->source.c_str());
		return;
	}

	std::string output_text;
	ServerInstance->Parser->TranslateUIDs(thiscmd->translation, parameters, output_text, true, thiscmd);

	parameterlist params;
	params.push_back(output_text);

	if (routing.type == ROUTE_TYPE_BROADCAST)
		Utils->DoOneToMany(user->uuid, command, params);
	else
		Utils->DoOneToOne(user->uuid, command, params, routing.serverdest);
}
