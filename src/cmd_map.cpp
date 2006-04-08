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
#include "commands.h"
#include "helperfuncs.h"
#include "cmd_map.h"

extern ServerConfig* Config;

void cmd_map::Handle (char **parameters, int pcnt, userrec *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.
	WriteServ(user->fd,"006 %s :%s",user->nick,Config->ServerName);
	WriteServ(user->fd,"007 %s :End of /MAP",user->nick);
}
