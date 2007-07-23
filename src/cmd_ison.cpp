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
#include "users.h"
#include "commands/cmd_ison.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
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
			if (u->Visibility && !u->Visibility->VisibleTo(user))
				continue;

			reply.append(u->nick).append(" ");
			if (reply.length() > 450)
			{
				user->WriteServ(reply);
				reply = std::string("303 ") + user->nick + " :";
			}
			ison_already[u] = u;
		}
		else
		{
			if ((i == pcnt-1) && (strchr(parameters[i],' ')))
			{
				/* Its a space seperated list of nicks (RFC1459 says to support this)
				 */
				irc::spacesepstream list(parameters[i]);
				std::string item("*");
				while (((item = list.GetToken()) != ""))
				{
					u = ServerInstance->FindNick(item);
					if (ison_already.find(u) != ison_already.end())
						continue;

					if (u)
					{
						if (u->Visibility && !u->Visibility->VisibleTo(user))
							continue;

						reply.append(u->nick).append(" ");
						if (reply.length() > 450)
						{
							user->WriteServ(reply);
							reply = std::string("303 ") + user->nick + " :";
						}
						ison_already[u] = u;
					}
				}
			}
			/* There will only be one of these, we can bail after. */
			break;
		}
	}

	if (!reply.empty())
		user->WriteServ(reply);

	return CMD_SUCCESS;
}

