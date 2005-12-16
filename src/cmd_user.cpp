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
#include "cmd_user.h"

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

void cmd_user::Handle (char **parameters, int pcnt, userrec *user)
{
	log(DEBUG,"Handling USER from cmd_user class!");
	if (user->registered < 3)
	{
		if (isident(parameters[0]) == 0) {
			// This kinda Sucks, According to the RFC thou, its either this,
			// or "You have already registered" :p -- Craig
			WriteServ(user->fd,"461 %s USER :Not enough parameters",user->nick);
		}
		else {
			/* We're not checking ident, but I'm not sure I like the idea of '~' prefixing.. */
			/* XXX - Should this IDENTMAX + 1 be IDENTMAX - 1? Ok, users.h has it defined as
			 * char ident[IDENTMAX+2]; - WTF?
			 */
			snprintf(user->ident, IDENTMAX+1, "~%s", parameters[0]);
			strlcpy(user->fullname,parameters[3],MAXGECOS);
			user->registered = (user->registered | 1);
		}
	}
	else
	{
		WriteServ(user->fd,"462 %s :You may not reregister",user->nick);
		return;
	}
	/* parameters 2 and 3 are local and remote hosts, ignored when sent by client connection */
	if (user->registered == 3)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD OnUserRegister(user);
		ConnectUser(user);
	}
}
	

