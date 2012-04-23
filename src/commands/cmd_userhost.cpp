/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
