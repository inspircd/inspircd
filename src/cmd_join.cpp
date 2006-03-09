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
#include "cmd_join.h"

extern InspIRCd* ServerInstance;

void cmd_join::Handle (char **parameters, int pcnt, userrec *user)
{
	if (ServerInstance->Parser->LoopCall(this, parameters, pcnt, user, 0, 0, 1))
		return;

	if (IsValidChannelName(parameters[0]))
	{
		add_channel(user, parameters[0], parameters[1], false);
	}
	else
	{
		WriteServ(user->fd,"403 %s %s :Invalid channel name",user->nick, parameters[0]);
	}
}



