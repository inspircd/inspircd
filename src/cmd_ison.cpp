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
	std::map<userrec*,userrec*> ison_already;
	userrec *u;
	std::string reply = std::string("303 ") + user->nick + " :";

	for (int i = 0; i < pcnt; i++)
	{
		u = ServerInstance->FindNick(parameters[i]);
		if (ison_already.find(u) != ison_already.end())
			continue;

		if (u)
		{
			reply.append(u->nick).append(" ");
			if (reply.length() > 450)
			{
				user->WriteServ(reply);
				reply = std::string("303 ") + user->nick + " :";
			}
			ison_already[u] = u;
		}
	}

	if (!reply.empty())
		user->WriteServ(reply);

	return CMD_SUCCESS;
}

