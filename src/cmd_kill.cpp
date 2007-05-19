/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "wildcard.h"
#include "commands/cmd_kill.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_kill(Instance);
}

/** Handle /KILL
 */
CmdResult cmd_kill::Handle (const char** parameters, int pcnt, userrec *user)
{
	/* Allow comma seperated lists of users for /KILL (thanks w00t) */
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	userrec *u = ServerInstance->FindNick(parameters[0]);
	char killreason[MAXBUF];
	char killoperreason[MAXBUF];
	int MOD_RESULT = 0;

	if (u)
	{
		FOREACH_RESULT(I_OnKill, OnKill(user, u, parameters[1]));

		if (MOD_RESULT)
			return CMD_FAILURE;

		// generate two reasons here, one for users, one for opers. first, the user visible reason, which may change.
		if (*ServerInstance->Config->HideKillsServer)
		{
			// hidekills is on, use it
			snprintf(killreason, MAXQUIT, "Killed (%s (%s))", ServerInstance->Config->HideKillsServer, parameters[1]);
		}
		else
		{
			// hidekills is off, do nothing
			snprintf(killreason, MAXQUIT, "Killed (%s (%s))", user->nick, parameters[1]);
		}

		// opers are lucky ducks, they always see the real reason
		snprintf(killoperreason, MAXQUIT, "Killed (%s (%s))", user->nick, parameters[1]);

		if (!IS_LOCAL(u))
		{
			// remote kill
			ServerInstance->SNO->WriteToSnoMask('k',"Remote kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			FOREACH_MOD(I_OnRemoteKill, OnRemoteKill(user, u, killreason));

			/*
			 * IMPORTANT SHIT:
			 *  There used to be a WriteCommonExcept() of the QUIT here. It seems to be unnecessary with QuitUser() right below, so it's gone.
			 *  If it explodes painfully, put it back!
			 */

			userrec::QuitUser(ServerInstance, u, killreason);
		}
		else
		{
			// local kill
			ServerInstance->SNO->WriteToSnoMask('k',"Local Kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			ServerInstance->Log(DEFAULT,"LOCAL KILL: %s :%s!%s!%s (%s)", u->nick, ServerInstance->Config->ServerName, user->dhost, user->nick, parameters[1]);
			user->WriteTo(u, "KILL %s :%s!%s!%s (%s)", u->nick, ServerInstance->Config->ServerName, user->dhost, user->nick, parameters[1]);
		}

		// send the quit out
		userrec::QuitUser(ServerInstance, u, killreason, killoperreason);
	}
	else
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick, parameters[0]);
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

