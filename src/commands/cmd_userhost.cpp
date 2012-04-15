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

#ifndef CMD_USERHOST_H
#define CMD_USERHOST_H

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /USERHOST. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandUserhost : public Command
{
 public:
	/** Constructor for userhost.
	 */
	CommandUserhost ( Module* parent) : Command(parent,"USERHOST",0,1) { syntax = "<nick>{,<nick>}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


CmdResult CommandUserhost::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string retbuf = std::string("302 ") + user->nick + " :";

	for (unsigned int i = 0; i < parameters.size(); i++)
	{
		User *u = ServerInstance->FindNickOnly(parameters[i]);

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

COMMAND_INIT(CommandUserhost)
