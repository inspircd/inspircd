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
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_kline.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;
extern userrec* fd_ref_table[65536];

void cmd_kline::Handle (char **parameters, int pcnt, userrec *user)
{
	if (pcnt >= 3)
	{
		if (host_matches_everyone(parameters[0],user))
			return;
		add_kline(duration(parameters[1]),user->nick,parameters[2],parameters[0]);
		FOREACH_MOD OnAddKLine(duration(parameters[1]), user, parameters[2], parameters[0]);
		if (!duration(parameters[1]))
		{
			WriteOpers("*** %s added permenant K-line for %s.",user->nick,parameters[0]);
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
			FOREACH_MOD OnDelKLine(user, parameters[0]);
			WriteOpers("*** %s Removed K-line on %s.",user->nick,parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"NOTICE %s :*** K-Line %s not found in list, try /stats k.",user->nick,parameters[0]);
		}
	}
}


