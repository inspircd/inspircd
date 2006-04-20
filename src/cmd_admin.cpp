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
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_admin.h"

extern ServerConfig* Config;

void cmd_admin::Handle (char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"256 %s :Administrative info for %s",user->nick,Config->ServerName);
	WriteServ(user->fd,"257 %s :Name     - %s",user->nick,Config->AdminName);
	WriteServ(user->fd,"258 %s :Nickname - %s",user->nick,Config->AdminNick);
	WriteServ(user->fd,"258 %s :E-Mail   - %s",user->nick,Config->AdminEmail);
}
