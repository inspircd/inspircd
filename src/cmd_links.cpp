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
#include "modules.h"
#include "commands/cmd_links.h"

void cmd_links::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("364 %s %s %s :0 %s",user->nick,ServerInstance->Config->ServerName,ServerInstance->Config->ServerName,ServerInstance->Config->ServerDesc);
	user->WriteServ("365 %s * :End of /LINKS list.",user->nick);
}
