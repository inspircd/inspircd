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
#include "commands/cmd_server.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandServer(Instance);
}

CmdResult CommandServer::Handle (const std::vector<std::string>&, User *user)
{
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "%s :You are already registered. (Perhaps your IRC client does not have a /SERVER command).",user->nick.c_str());
	}
	else
	{
		user->WriteNumeric(ERR_NOTREGISTERED, "%s :You may not register as a server (servers have seperate ports from clients, change your config)",command.c_str());
	}
	return CMD_FAILURE;
}
