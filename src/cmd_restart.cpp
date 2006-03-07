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
#include "cmd_restart.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

void cmd_restart::Handle (char **parameters, int pcnt, userrec *user)
{
	char *argv[32];
	log(DEFAULT,"Restart: %s",user->nick);
	if (!strcmp(parameters[0],Config->restartpass))
	{
		WriteOpers("*** RESTART command from %s!%s@%s, restarting server.",user->nick,user->ident,user->host);

		argv[0] = Config->MyExecutable;
		argv[1] = "-wait";
		if (Config->nofork)
		{
			argv[2] = "-nofork";
		}
		else
		{
			argv[2] = NULL;
		}
		argv[3] = NULL;
		
		// close ALL file descriptors
		send_error("Server restarting.");
		sleep(1);
		for (int i = 0; i < MAX_DESCRIPTORS; i++)
		{
			shutdown(i,2);
    			close(i);
		}
		sleep(2);
		
		execv(Config->MyExecutable,argv);

		exit(0);
	}
	else
	{
		WriteOpers("*** Failed RESTART Command from %s!%s@%s.",user->nick,user->ident,user->host);
	}
}


