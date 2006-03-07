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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "message.h"
#include "commands.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "command_parse.h"
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

