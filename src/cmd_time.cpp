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

#include <time.h>
#include "configreader.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_time.h"


extern time_t TIME;

void cmd_time::Handle (const char** parameters, int pcnt, userrec *user)
{
	struct tm* timeinfo;

	timeinfo = localtime(&TIME);

	char tms[26];
	snprintf(tms,26,"%s",asctime(timeinfo));
	tms[24] = 0;

	user->WriteServ("391 %s %s :%s",user->nick,ServerInstance->Config->ServerName,tms);
  
}
