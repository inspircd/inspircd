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
#include "commands/cmd_admin.h"


extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandAdmin(Instance);
}

/** Handle /ADMIN
 */
CmdResult CommandAdmin::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteNumeric(RPL_ADMINME, "%s :Administrative info for %s",user->nick.c_str(),ServerInstance->Config->ServerName);
	if (*ServerInstance->Config->AdminName)
		user->WriteNumeric(RPL_ADMINLOC1, "%s :Name     - %s",user->nick.c_str(),ServerInstance->Config->AdminName);
	user->WriteNumeric(RPL_ADMINLOC2, "%s :Nickname - %s",user->nick.c_str(),ServerInstance->Config->AdminNick);
	user->WriteNumeric(RPL_ADMINEMAIL, "%s :E-Mail   - %s",user->nick.c_str(),ServerInstance->Config->AdminEmail);
	return CMD_SUCCESS;
}
