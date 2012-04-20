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
#include "commands/cmd_lusers.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandLusers(Instance);
}

/** Handle /LUSERS
 */
CmdResult CommandLusers::Handle (const std::vector<std::string>&, User *user)
{
	// this lusers command shows one server at all times because
	// a protocol module must override it to show those stats.
	user->WriteNumeric(251, "%s :There are %d users and %d invisible on 1 server",user->nick.c_str(),ServerInstance->Users->UserCount()-ServerInstance->Users->ModeCount('i'),ServerInstance->Users->ModeCount('i'));
	if (ServerInstance->Users->OperCount())
		user->WriteNumeric(252, "%s %d :operator(s) online",user->nick.c_str(),ServerInstance->Users->OperCount());
	if (ServerInstance->Users->UnregisteredUserCount())
		user->WriteNumeric(253, "%s %d :unknown connections",user->nick.c_str(),ServerInstance->Users->UnregisteredUserCount());
	if (ServerInstance->ChannelCount())
		user->WriteNumeric(254, "%s %ld :channels formed",user->nick.c_str(),ServerInstance->ChannelCount());
	if (ServerInstance->Users->LocalUserCount())
		user->WriteNumeric(255, "%s :I have %d clients and 0 servers",user->nick.c_str(),ServerInstance->Users->LocalUserCount());

	return CMD_SUCCESS;
}

