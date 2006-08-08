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

#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_server.h"

void cmd_server::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("666 %s :You cannot identify as a server, you are a USER. IRC Operators informed.",user->nick);
	WriteOpers("*** WARNING: %s attempted to issue a SERVER command and is registered as a user!",user->nick);
}
