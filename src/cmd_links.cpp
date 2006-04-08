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

#include "inspircd_config.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "helperfuncs.h"
#include "cmd_links.h"

extern ServerConfig* Config;

void cmd_links::Handle (char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"364 %s %s %s :0 %s",user->nick,Config->ServerName,Config->ServerName,Config->ServerDesc);
	WriteServ(user->fd,"365 %s * :End of /LINKS list.",user->nick);
}
