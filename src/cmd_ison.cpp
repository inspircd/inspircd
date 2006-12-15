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

#include "users.h"
#include "inspircd.h"
#include "commands/cmd_ison.h"

extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_ison(Instance);
}

/** Handle /ISON
 */
CmdResult cmd_ison::Handle (const char** parameters, int pcnt, userrec *user)
{
	char retbuf[MAXBUF];
	userrec *u;

	snprintf(retbuf, MAXBUF, "303 %s :", user->nick);

	for (int i = 0; i < pcnt; i++)
	{
		u = ServerInstance->FindNick(parameters[i]);

		if (u)
		{
			strlcat(retbuf, u->nick, MAXBUF);
			charlcat(retbuf, ' ', MAXBUF);
		}
	}

	user->WriteServ(retbuf);

	return CMD_SUCCESS;
}

