/*   +------------------------------------+
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

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "commands/cmd_version.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_version(Instance);
}

CmdResult cmd_version::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("351 %s :%s",user->nick,ServerInstance->GetVersionString().c_str());
	ServerInstance->Config->Send005(user);
	return CMD_SUCCESS;
}
