/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "commands.h"

CmdResult CommandSVSJoin::Handle(const std::vector<std::string>& parameters, User *user)
{
	// Check for valid channel name
	if (!ServerInstance->IsChannel(parameters[1].c_str(), ServerInstance->Config->Limits.ChanMax))
		return CMD_FAILURE;

	// Check target exists
	User* u = ServerInstance->FindNick(parameters[0]);
	if (!u)
		return CMD_FAILURE;

	/* only join if it's local, otherwise just pass it on! */
	if (IS_LOCAL(u))
		Channel::JoinUser(u, parameters[1].c_str(), false, "", false, ServerInstance->Time());
	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSJoin::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	User* u = ServerInstance->FindNick(parameters[0]);
	if (u)
		return ROUTE_OPT_UCAST(u->server);
	return ROUTE_LOCALONLY;
}
