/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands/cmd_trace.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandTrace(Instance);
}

/** XXX: This is crap. someone fix this when you have time, to be more useful.
 */
CmdResult CommandTrace::Handle (const std::vector<std::string>&, User *user)
{
	/*for (user_hash::iterator i = ServerInstance->clientlist->begin(); i != ServerInstance->clientlist->end(); i++)
	{
		if (i->second->registered == REG_ALL)
		{
			if (IS_OPER(i->second))
			{
				user->WriteNumeric(205, "%s :Oper 0 %s",user->nick,i->second->nick);
			}
			else
			{
				user->WriteNumeric(204, "%s :User 0 %s",user->nick,i->second->nick);
			}
		}
		else
		{
			user->WriteNumeric(203, "%s :???? 0 [%s]",user->nick,i->second->host);
		}
	}*/
	return CMD_SUCCESS;
}
