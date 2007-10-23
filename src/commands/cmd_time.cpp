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
#include "commands/cmd_time.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandTime(Instance);
}

CmdResult CommandTime::Handle (const char**, int, User *user)
{
	struct tm* timeinfo;
	time_t local = ServerInstance->Time();

	timeinfo = localtime(&local);

	char tms[26];
	snprintf(tms,26,"%s",asctime(timeinfo));
	tms[24] = 0;

	user->WriteServ("391 %s %s :%s",user->nick,ServerInstance->Config->ServerName,tms);

	return CMD_SUCCESS;
}
