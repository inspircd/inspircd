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

#ifndef CMD_ISON_H
#define CMD_ISON_H

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /ISON. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandIson : public Command
{
 public:
	/** Constructor for ison.
	 */
	CommandIson ( Module* parent) : Command(parent,"ISON",0,0) { syntax = "<nick> {nick}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /ISON
 */
CmdResult CommandIson::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::map<User*,User*> ison_already;
	User *u;
	std::string reply = std::string("303 ") + user->nick + " :";

	for (unsigned int i = 0; i < parameters.size(); i++)
	{
		u = ServerInstance->FindNickOnly(parameters[i]);
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
					u = ServerInstance->FindNickOnly(item);
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
			}
		}
	}

	if (!reply.empty())
		user->WriteServ(reply);

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandIson)
