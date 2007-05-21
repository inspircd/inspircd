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
#include "commands/cmd_clearcache.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_clearcache(Instance);
}

/** Handle /CLEARCACHE
 */
CmdResult cmd_clearcache::Handle (const char** parameters, int pcnt, userrec *user)
{
	int n = ServerInstance->Res->ClearCache();
	user->WriteServ("NOTICE %s :*** Cleared DNS cache of %d items.", user->nick, n);
	return CMD_SUCCESS;
}
