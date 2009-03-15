/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_userhost.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandUserhost(Instance);
}

CmdResult CommandUserhost::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string retbuf = std::string("302 ") + user->nick + " :";

	for (unsigned int i = 0; i < parameters.size(); i++)
	{
		User *u = ServerInstance->FindNick(parameters[i]);

		if ((u) && (u->registered == REG_ALL))
		{
			retbuf = retbuf + u->nick;

			if (IS_OPER(u))
			{
				retbuf = retbuf + "*=";
			}
			else
			{
				retbuf = retbuf + "=";
			}

			if (IS_AWAY(u))
				retbuf += "-";
			else
				retbuf += "+";

			retbuf = retbuf + u->ident + "@";

			if (user->HasPrivPermission("users/auspex"))
			{
				retbuf = retbuf + u->host;
			}
			else
			{
				retbuf = retbuf + u->dhost;
			}

			retbuf = retbuf + " ";
		}
	}

	user->WriteServ(retbuf);

	return CMD_SUCCESS;
}
