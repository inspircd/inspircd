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
#include "cmd_trace.h"

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

void cmd_trace::Handle (char **parameters, int pcnt, userrec *user)
{
	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (isnick(i->second->nick))
			{
				if (strchr(i->second->modes,'o'))
				{
					WriteServ(user->fd,"205 %s :Oper 0 %s",user->nick,i->second->nick);
				}
				else
				{
					WriteServ(user->fd,"204 %s :User 0 %s",user->nick,i->second->nick);
				}
			}
			else
			{
				WriteServ(user->fd,"203 %s :???? 0 [%s]",user->nick,i->second->host);
			}
		}
	}
}


