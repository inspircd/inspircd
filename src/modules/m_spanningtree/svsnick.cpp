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

#include "main.h"
#include "utils.h"
#include "commands.h"

CmdResult CommandSVSNick::Handle(const std::vector<std::string>& parameters, User *user)
{
	User* u = ServerInstance->FindNick(parameters[0]);

	if (u && IS_LOCAL(u))
	{
		std::string nick = parameters[1];
		if (isdigit(nick[0]))
			nick = u->uuid;

		if (!u->ForceNickChange(nick.c_str()))
		{
			/* buh. UID them */
			if (!u->ForceNickChange(u->uuid.c_str()))
			{
				ServerInstance->Users->QuitUser(u, "Nickname collision");
				return CMD_SUCCESS;
			}
		}

		u->age = atoi(parameters[2].c_str());
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSNick::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	User* u = ServerInstance->FindNick(parameters[0]);
	if (u)
		return ROUTE_OPT_UCAST(u->server);
	return ROUTE_LOCALONLY;
}
