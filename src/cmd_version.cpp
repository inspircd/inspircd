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
#include "configreader.h"
#include "users.h"
#include "commands/cmd_version.h"



extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_version(Instance);
}

CmdResult cmd_version::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("351 %s :%s",user->nick,ServerInstance->GetVersionString().c_str());
	ServerInstance->Config->Send005(user);
	return CMD_SUCCESS;
}
