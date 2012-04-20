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
#include "commands/cmd_map.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMap(Instance);
}

/** Handle /MAP
 */
CmdResult CommandMap::Handle (const std::vector<std::string>&, User *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.
	user->WriteNumeric(006, "%s :%s",user->nick.c_str(),ServerInstance->Config->ServerName);
	user->WriteNumeric(007, "%s :End of /MAP",user->nick.c_str());

	return CMD_SUCCESS;
}
