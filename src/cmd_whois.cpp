/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *		<Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <time.h>
#include <string>
#include <ext/hash_map>
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
#include "cmd_whois.h"

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

void cmd_whois::Handle (char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-1,0))
		return;
	dest = Find(parameters[0]);
	if (dest)
	{
		do_whois(user,dest,0,0,parameters[0]);
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, parameters[0]);
	}
}

