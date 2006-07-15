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
#include <string>
#include <ext/hash_map>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cmd_kill.h"

extern ServerConfig* Config;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern user_hash clientlist;

void cmd_kill::Handle (char **parameters, int pcnt, userrec *user)
{
	userrec *u = Find(parameters[0]);
	char killreason[MAXBUF];
	int MOD_RESULT = 0;

	log(DEBUG,"kill: %s %s", parameters[0], parameters[1]);

	if (u)
	{
		log(DEBUG, "into kill mechanism");
		FOREACH_RESULT(I_OnKill, OnKill(user, u, parameters[1]));

		if (MOD_RESULT)
		{
			log(DEBUG, "A module prevented the kill with result %d", MOD_RESULT);
			return;
		}

		if (!IS_LOCAL(u))
		{
			// remote kill
			WriteOpers("*** Remote kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			snprintf(killreason, MAXQUIT,"[%s] Killed (%s (%s))", Config->ServerName, user->nick, parameters[1]);
			WriteCommonExcept(u, "QUIT :%s", killreason);
			FOREACH_MOD(I_OnRemoteKill, OnRemoteKill(user, u, killreason));
			
			user_hash::iterator iter = clientlist.find(u->nick);

			if (iter != clientlist.end())
			{
				log(DEBUG,"deleting user hash value %d", iter->second);
				clientlist.erase(iter);
			}

			if (u->registered == 7)
			{
				purge_empty_chans(u);
			}

			delete u;
		}
		else
		{
			// local kill
			log(DEFAULT,"LOCAL KILL: %s :%s!%s!%s (%s)", u->nick, Config->ServerName, user->dhost, user->nick, parameters[1]);
			WriteTo(user, u, "KILL %s :%s!%s!%s (%s)", u->nick, Config->ServerName, user->dhost, user->nick, parameters[1]);
			WriteOpers("*** Local Kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			snprintf(killreason,MAXQUIT,"Killed (%s (%s))", user->nick, parameters[1]);
			kill_link(u, killreason);
		}
	}
	else
	{
		WriteServ(user->fd, "401 %s %s :No such nick/channel", user->nick, parameters[0]);
	}
}


