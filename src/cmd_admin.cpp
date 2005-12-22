/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain.net>
 *                <Craig.net>
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
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cmd_admin.h"

extern ServerConfig* Config;

void cmd_admin::Handle (char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"256 %s :Administrative info for %s",user->nick,Config->ServerName);
	WriteServ(user->fd,"257 %s :Name     - %s",user->nick,Config->AdminName);
	WriteServ(user->fd,"258 %s :Nickname - %s",user->nick,Config->AdminNick);
	WriteServ(user->fd,"258 %s :E-Mail   - %s",user->nick,Config->AdminEmail);
}


