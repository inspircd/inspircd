/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "users.h"
#include "commands/cmd_admin.h"

extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_admin(Instance);
}

/** Handle /ADMIN
 */
CmdResult cmd_admin::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("256 %s :Administrative info for %s",user->nick,ServerInstance->Config->ServerName);
	user->WriteServ("257 %s :Name     - %s",user->nick,ServerInstance->Config->AdminName);
	user->WriteServ("258 %s :Nickname - %s",user->nick,ServerInstance->Config->AdminNick);
	user->WriteServ("258 %s :E-Mail   - %s",user->nick,ServerInstance->Config->AdminEmail);
	return CMD_SUCCESS;
}
