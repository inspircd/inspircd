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

