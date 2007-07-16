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
#include "commands/cmd_time.h"



extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_time(Instance);
}

CmdResult cmd_time::Handle (const char** parameters, int pcnt, userrec *user)
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
