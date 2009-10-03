/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /KILL. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandKill : public Command
{
 public:
	/** Constructor for kill.
	 */
	CommandKill ( Module* parent) : Command(parent,"KILL",2,2) { flags_needed = 'o'; syntax = "<nickname> <reason>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /KILL
 */
CmdResult CommandKill::Handle (const std::vector<std::string>& parameters, User *user)
{
	/* Allow comma seperated lists of users for /KILL (thanks w00t) */
	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	User *u = ServerInstance->FindNick(parameters[0]);
	char killreason[MAXBUF];
	ModResult MOD_RESULT;

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
			FIRST_MOD_RESULT(OnKill, MOD_RESULT, (user, u, parameters[1]));

			if (MOD_RESULT == MOD_RES_DENY)
				return CMD_FAILURE;

			if (!ServerInstance->Config->HideKillsServer.empty())
			{
				// hidekills is on, use it
				snprintf(killreason, ServerInstance->Config->Limits.MaxQuit, "Killed (%s (%s))", ServerInstance->Config->HideKillsServer.c_str(), parameters[1].c_str());
			}
			else
			{
				// hidekills is off, do nothing
				snprintf(killreason, ServerInstance->Config->Limits.MaxQuit, "Killed (%s (%s))", user->nick.c_str(), parameters[1].c_str());
			}
		}
		else
		{
			/* Leave it alone, remote server has already formatted it */
			strlcpy(killreason, parameters[1].c_str(), ServerInstance->Config->Limits.MaxQuit);
		}

		/*
		 * Now we need to decide whether or not to send a local or remote snotice. Currently this checking is a little flawed.
		 * No time to fix it right now, so left a note. -- w00t
		 */
		if (!IS_LOCAL(u))
		{
			// remote kill
			ServerInstance->SNO->WriteToSnoMask('K', "Remote kill by %s: %s!%s@%s (%s)", user->nick.c_str(), u->nick.c_str(), u->ident.c_str(), u->host.c_str(), parameters[1].c_str());
			FOREACH_MOD(I_OnRemoteKill, OnRemoteKill(user, u, killreason, killreason));
		}
		else
		{
			// local kill
			/*
			 * XXX - this isn't entirely correct, servers A - B - C, oper on A, client on C. Oper kills client, A and B will get remote kill
			 * snotices, C will get a local kill snotice. this isn't accurate, and needs fixing at some stage. -- w00t
			 */
			ServerInstance->SNO->WriteToSnoMask('k',"Local Kill by %s: %s!%s@%s (%s)", user->nick.c_str(), u->nick.c_str(), u->ident.c_str(), u->host.c_str(), parameters[1].c_str());
			ServerInstance->Logs->Log("KILL",DEFAULT,"LOCAL KILL: %s :%s!%s!%s (%s)", u->nick.c_str(), ServerInstance->Config->ServerName.c_str(), user->dhost.c_str(), user->nick.c_str(), parameters[1].c_str());
			/* Bug #419, make sure this message can only occur once even in the case of multiple KILL messages crossing the network, and change to show
			 * hidekillsserver as source if possible
			 */
			if (!u->quitting)
			{
				u->Write(":%s KILL %s :%s!%s!%s (%s)", ServerInstance->Config->HideKillsServer.empty() ? user->GetFullHost().c_str() : ServerInstance->Config->HideKillsServer.c_str(),
						u->nick.c_str(),
						ServerInstance->Config->ServerName.c_str(),
						user->dhost.c_str(),
						ServerInstance->Config->HideKillsServer.empty() ? user->nick.c_str() : ServerInstance->Config->HideKillsServer.c_str(),
						parameters[1].c_str());
			}
		}

		// send the quit out
		ServerInstance->Users->QuitUser(u, killreason);
	}
	else
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandKill)
