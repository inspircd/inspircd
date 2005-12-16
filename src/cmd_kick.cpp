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
#include "cmd_kick.h"

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

void cmd_kick::Handle (char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr = FindChan(parameters[0]);
	userrec* u   = Find(parameters[1]);

	if ((!u) || (!Ptr))
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		return;
	}
	
	if ((!has_channel(user,Ptr)) && (!is_uline(user->server)))
	{
		WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, parameters[0]);
		return;
	}
	
	char reason[MAXBUF];
	
	if (pcnt > 2)
	{
		strlcpy(reason,parameters[2],MAXBUF);
		if (strlen(reason)>MAXKICK)
		{
			reason[MAXKICK-1] = '\0';
		}

		kick_channel(user,u,Ptr,reason);
	}
	else
	{
		strlcpy(reason,user->nick,MAXBUF);
		kick_channel(user,u,Ptr,reason);
	}
	
}


