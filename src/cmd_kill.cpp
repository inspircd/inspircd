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
		/*
		 * Here, we need to decide how to munge kill messages. Whether to hide killer, what to show opers, etc.
		 * We only do this when the command is being issued LOCALLY, for remote KILL, we just copy the message we got.
		 *
		 * This conditional is so that we only append the "Killed (" prefix ONCE. If killer is remote, then the kill
		 * just gets processed and passed on, otherwise, if they are local, it gets prefixed. Makes sense :-) -- w00t
		 */
		if (IS_LOCAL(user))
		{
			/*
			 * Moved this event inside the IS_LOCAL check also, we don't want half the network killing a user
			 * and the other half not. This would be a bad thing. ;p -- w00t
			 */
			FOREACH_RESULT(I_OnKill, OnKill(user, u, parameters[1]));

			if (MOD_RESULT)
				return CMD_FAILURE;

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
		}
		else
		{
			snprintf(killreason, MAXQUIT, "%s", parameters[1]);
			/*
			 * XXX - yes, this means opers will probably see a censored kill remotely. this needs fixing.
			 * maybe a version of QuitUser that doesn't take nor propegate an oper reason? -- w00t
			 */
			snprintf(killoperreason, MAXQUIT, "%s", parameters[1]);
		}

		/*
		 * Now we need to decide whether or not to send a local or remote snotice. Currently this checking is a little flawed.
		 * No time to fix it right now, so left a note. -- w00t
		 */
		if (!IS_LOCAL(u))
		{
			// remote kill
			ServerInstance->SNO->WriteToSnoMask('K', "Remote kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			FOREACH_MOD(I_OnRemoteKill, OnRemoteKill(user, u, killreason, killoperreason));
		}
		else
		{
			// local kill
			/*
			 * XXX - this isn't entirely correct, servers A - B - C, oper on A, client on C. Oper kills client, A and B will get remote kill
			 * snotices, C will get a local kill snotice. this isn't accurate, and needs fixing at some stage. -- w00t
			 */
			ServerInstance->SNO->WriteToSnoMask('k',"Local Kill by %s: %s!%s@%s (%s)", user->nick, u->nick, u->ident, u->host, parameters[1]);
			ServerInstance->Log(DEFAULT,"LOCAL KILL: %s :%s!%s!%s (%s)", u->nick, ServerInstance->Config->ServerName, user->dhost, user->nick, parameters[1]);
			user->WriteTo(u, "KILL %s :%s!%s!%s (%s)", u->nick, ServerInstance->Config->ServerName, user->dhost,
					*ServerInstance->Config->HideKillsServer ? ServerInstance->Config->HideKillsServer : user->nick, parameters[1]);
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

