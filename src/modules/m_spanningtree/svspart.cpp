/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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

CmdResult CommandSVSPart::Handle(const std::vector<std::string>& parameters, User *user)
{
	std::string reason = "Services forced part";

	if (parameters.size() == 3)
		reason = parameters[2];

	User* u = ServerInstance->FindNick(parameters[0]);
	Channel* c = ServerInstance->FindChan(parameters[1]);

	if (u && IS_LOCAL(u))
		c->PartUser(u, reason);

	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSPart::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	User* u = ServerInstance->FindNick(parameters[0]);
	if (u)
		return ROUTE_OPT_UCAST(u->server);
	return ROUTE_LOCALONLY;
}
