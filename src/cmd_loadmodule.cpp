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
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cmd_loadmodule.h"

extern InspIRCd* ServerInstance;

void cmd_loadmodule::Handle (char **parameters, int pcnt, userrec *user)
{
	if (ServerInstance->LoadModule(parameters[0]))
	{
		WriteOpers("*** NEW MODULE: %s",parameters[0]);
		WriteServ(user->fd,"975 %s %s :Module successfully loaded.",user->nick, parameters[0]);
	}
	else
	{
		WriteServ(user->fd,"974 %s %s :Failed to load module: %s",user->nick, parameters[0],ServerInstance->ModuleError());
	}
}


