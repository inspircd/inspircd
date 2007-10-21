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
#include "commands/cmd_clearcache.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandClearcache(Instance);
}

/** Handle /CLEARCACHE
 */
CmdResult CommandClearcache::Handle (const char** parameters, int pcnt, User *user)
{
	int n = ServerInstance->Res->ClearCache();
	user->WriteServ("NOTICE %s :*** Cleared DNS cache of %d items.", user->nick, n);
	return CMD_SUCCESS;
}
