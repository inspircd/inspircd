/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_lusers.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandLusers(Instance);
}

/** Handle /LUSERS
 */
CmdResult CommandLusers::Handle (const char**, int, User *user)
{
	// this lusers command shows one server at all times because
	// a protocol module must override it to show those stats.
	user->WriteServ("251 %s :There are %d users and %d invisible on 1 server",user->nick,ServerInstance->UserCount()-ServerInstance->ModeCount('i'),ServerInstance->ModeCount('i'));
	if (ServerInstance->OperCount())
		user->WriteServ("252 %s %d :operator(s) online",user->nick,ServerInstance->OperCount());
	if (ServerInstance->UnregisteredUserCount())
		user->WriteServ("253 %s %d :unknown connections",user->nick,ServerInstance->UnregisteredUserCount());
	if (ServerInstance->ChannelCount())
		user->WriteServ("254 %s %d :channels formed",user->nick,ServerInstance->ChannelCount());
	if (ServerInstance->LocalUserCount())
		user->WriteServ("255 %s :I have %d clients and 0 servers",user->nick,ServerInstance->LocalUserCount());

	return CMD_SUCCESS;
}

