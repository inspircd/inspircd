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
#include "cmd_whowas.h"

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

extern whowas_users whowas;

void cmd_whowas::Handle (char **parameters, int pcnt, userrec* user)
{
	whowas_users::iterator i = whowas.find(parameters[0]);

	if (i == whowas.end())
	{
		WriteServ(user->fd,"406 %s %s :There was no such nickname",user->nick,parameters[0]);
	}
	else
	{
		whowas_set* grp = i->second;
		if (grp->size())
		{
			for (whowas_set::iterator ux = grp->begin(); ux != grp->end(); ux++)
			{
				WhoWasGroup* u = *ux;
				time_t rawtime = u->signon;
				tm *timeinfo;
				char b[MAXBUF];
	
				timeinfo = localtime(&rawtime);
				
				/* XXX - 'b' could be only 25 chars long and then strlcpy() would terminate it for us too? */
				strlcpy(b,asctime(timeinfo),MAXBUF);
				b[24] = 0;

				WriteServ(user->fd,"314 %s %s %s %s * :%s",user->nick,parameters[0],u->ident,u->dhost,u->gecos);
				
				if(*user->oper)
					WriteServ(user->fd,"379 %s %s :was connecting from *@%s", user->nick, parameters[0], u->host);
				
				if(*Config->HideWhoisServer && !(*user->oper))
					WriteServ(user->fd,"312 %s %s %s :%s",user->nick,parameters[0], Config->HideWhoisServer, b);
				else
					WriteServ(user->fd,"312 %s %s %s :%s",user->nick,parameters[0], u->server, b);
			}
		}
		else
		{
			WriteServ(user->fd,"406 %s %s :There was no such nickname",user->nick,parameters[0]);
		}
	}
	
	WriteServ(user->fd,"369 %s %s :End of WHOWAS",user->nick,parameters[0]);
}
