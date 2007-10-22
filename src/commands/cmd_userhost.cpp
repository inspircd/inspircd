/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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

CmdResult CommandUserhost::Handle (const char** parameters, int pcnt, User *user)
{
	std::string retbuf = std::string("302 ") + user->nick + " :";

	
	for (int i = 0; i < pcnt; i++)
	{
		User *u = ServerInstance->FindNick(parameters[i]);

		if ((u) && (u->registered == REG_ALL))
		{
			retbuf = retbuf + u->nick;

			if (IS_OPER(u))
			{
				retbuf = retbuf + "*=+";
			}
			else
			{
				retbuf = retbuf + "=+";
			}

			retbuf = retbuf + u->ident + "@";

			if (IS_OPER(user))
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
