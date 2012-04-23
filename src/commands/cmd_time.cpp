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
#include "commands/cmd_time.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandTime(Instance);
}

CmdResult CommandTime::Handle (const std::vector<std::string>&, User *user)
{
	struct tm* timeinfo;
	time_t local = ServerInstance->Time();

	timeinfo = localtime(&local);

	char tms[26];
	snprintf(tms,26,"%s",asctime(timeinfo));
	tms[24] = 0;

	user->WriteNumeric(RPL_TIME, "%s %s :%s",user->nick.c_str(),ServerInstance->Config->ServerName,tms);

	return CMD_SUCCESS;
}
