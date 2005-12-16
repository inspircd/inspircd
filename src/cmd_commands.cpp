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
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_commands.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;

void cmd_commands::Handle (char **parameters, int pcnt, userrec *user)
{
	for (unsigned int i = 0; i < ServerInstance->Parser->cmdlist.size(); i++)
	{
		WriteServ(user->fd,"902 %s :%s %s %d",user->nick,ServerInstance->Parser->cmdlist[i]->command.c_str(),ServerInstance->Parser->cmdlist[i]->source.c_str(),ServerInstance->Parser->cmdlist[i]->min_params);
	}
	WriteServ(user->fd,"903 %s :End of COMMANDS list",user->nick);
}


