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

#include <time.h>
#include <string>
#include <map>
#include <sstream>
#include <vector>
#include <deque>

#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "xline.h"
#include "helperfuncs.h"
#include "commands/cmd_kline.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;

void cmd_kline::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (host_matches_everyone(parameters[0],user))
			return;

		add_kline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD(I_OnAddKLine,OnAddKLine(duration(parameters[1]), user, parameters[2], parameters[0]));

		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permanent K-line for %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteOpers("*** %s added timed K-line for %s, expires in %d seconds.",user->nick,parameters[0],duration(parameters[1]));
		}

		apply_lines(APPLY_KLINES);
	}
	else
	{
		if (del_kline(parameters[0]))
		{
			FOREACH_MOD(I_OnDelKLine,OnDelKLine(user, parameters[0]));
			WriteOpers("*** %s Removed K-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			user->WriteServ("NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}
}
