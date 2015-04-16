/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

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
	CommandUserhost ( Module* parent) : Command(parent,"USERHOST", 1) {
		syntax = "<nick> [<nick> ...]";
	}
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandUserhost::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string retbuf = "302 " + user->nick + " :";

	unsigned int max = parameters.size();
	if (max > 5)
		max = 5;

	bool has_privs = user->HasPrivPermission("users/auspex");
	for (unsigned int i = 0; i < max; i++)
	{
		User *u = ServerInstance->FindNickOnly(parameters[i]);

		if ((u) && (u->registered == REG_ALL))
		{
			retbuf = retbuf + u->nick;

			if (IS_OPER(u))
			{
				// XXX: +H hidden opers must not be shown as opers
				ModeHandler* mh = ServerInstance->Modes->FindMode('H', MODETYPE_USER);
				if ((u == user) || (has_privs) || (!mh) || (!u->IsModeSet('H')) || (mh->name != "hideoper"))
					retbuf += '*';
			}

			retbuf = retbuf + "=";

			if (IS_AWAY(u))
				retbuf += "-";
			else
				retbuf += "+";

			retbuf = retbuf + u->ident + "@";

			if (has_privs)
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
