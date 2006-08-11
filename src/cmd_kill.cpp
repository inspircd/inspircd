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

#include <vector>
#include "inspircd_config.h"
#include "hash_map.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_kill.h"



void cmd_kill::Handle (const char** parameters, int pcnt, userrec *user)
{
	userrec *u = ServerInstance->FindNick(parameters[0]);
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
			ServerInstance->WriteOpers("*** Remote kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			snprintf(killreason, MAXQUIT,"[%s] Killed (%s (%s))", ServerInstance->Config->ServerName, user->nick, parameters[1]);
			u->WriteCommonExcept("QUIT :%s", killreason);
			FOREACH_MOD(I_OnRemoteKill, OnRemoteKill(user, u, killreason));
			
			user_hash::iterator iter = ServerInstance->clientlist.find(u->nick);

			if (iter != ServerInstance->clientlist.end())
			{
				log(DEBUG,"deleting user hash value %d", iter->second);
				ServerInstance->clientlist.erase(iter);
			}

			if (u->registered == REG_ALL)
			{
				u->PurgeEmptyChannels();
			}

			DELETE(u);
		}
		else
		{
			// local kill
			log(DEFAULT,"LOCAL KILL: %s :%s!%s!%s (%s)", u->nick, ServerInstance->Config->ServerName, user->dhost, user->nick, parameters[1]);
			user->WriteTo(u, "KILL %s :%s!%s!%s (%s)", u->nick, ServerInstance->Config->ServerName, user->dhost, user->nick, parameters[1]);
			ServerInstance->WriteOpers("*** Local Kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			snprintf(killreason,MAXQUIT,"Killed (%s (%s))", user->nick, parameters[1]);
			userrec::QuitUser(ServerInstance, u, killreason);
		}
	}
	else
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick, parameters[0]);
	}
}
