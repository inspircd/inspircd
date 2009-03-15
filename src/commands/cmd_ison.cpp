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
#include "commands/cmd_ison.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandIson(Instance);
}

/** Handle /ISON
 */
CmdResult CommandIson::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::map<User*,User*> ison_already;
	User *u;
	std::string reply = std::string("303 ") + user->nick + " :";

	for (unsigned int i = 0; i < parameters.size(); i++)
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
			if ((i == parameters.size() - 1) && (parameters[i].find(' ') != std::string::npos))
			{
				/* Its a space seperated list of nicks (RFC1459 says to support this)
				 */
				irc::spacesepstream list(parameters[i]);
				std::string item;

				while (list.GetToken(item))
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
		}
	}

	if (!reply.empty())
		user->WriteServ(reply);

	return CMD_SUCCESS;
}

