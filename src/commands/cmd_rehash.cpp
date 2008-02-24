/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "commands/cmd_rehash.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandRehash(Instance);
}

CmdResult CommandRehash::Handle (const char* const* parameters, int pcnt, User *user)
{
	user->WriteServ("382 %s %s :Rehashing",user->nick,ServerConfig::CleanFilename(ServerInstance->ConfigFileName));
	std::string parameter;
	std::string old_disabled = ServerInstance->Config->DisabledCommands;
	if (pcnt)
	{
		parameter = parameters[0];
	}
	else
	{
		ServerInstance->SNO->WriteToSnoMask('A', "%s is rehashing config file %s",user->nick,ServerConfig::CleanFilename(ServerInstance->ConfigFileName));
		ServerInstance->Logs->CloseLogs();
		if (!ServerInstance->OpenLog(ServerInstance->Config->argv, ServerInstance->Config->argc))
			user->WriteServ("NOTICE %s :*** ERROR: Could not open logfile %s: %s", user->nick, ServerInstance->Config->logpath.c_str(), strerror(errno));
		ServerInstance->RehashUsersAndChans();
		FOREACH_MOD(I_OnGarbageCollect, OnGarbageCollect());
		if (!ServerInstance->ConfigThread)
		{
			ServerInstance->ConfigThread = new ConfigReaderThread(ServerInstance, false, user);
			ServerInstance->Threads->Create(ServerInstance->ConfigThread);
		}
		else
		{
			/* A rehash is already in progress! ahh shit. */
			user->WriteServ("*** NOTICE %s :*** Could not rehash: A rehash is already in progress.", user->nick);
			return CMD_FAILURE;
		}
		/* TODO:
		 * ALL THIS STUFF HERE NEEDS TO BE HOOKED TO THE 'DEATH' OF THE REHASH THREAD
		 * VIA SOME NOTIFICATION EVENT. WE CANT JUST CALL IT ALL HERE.
		 * -- B
		 */
		// Get XLine to do it's thing.
		/*ServerInstance->XLines->CheckELines();
		ServerInstance->XLines->ApplyLines();
		ServerInstance->Res->Rehash();
		ServerInstance->ResetMaxBans();*/
	}

	/* TODO: Same as above for all this stuff, really */
	if (old_disabled != ServerInstance->Config->DisabledCommands)
		InitializeDisabledCommands(ServerInstance->Config->DisabledCommands, ServerInstance);

	FOREACH_MOD(I_OnRehash,OnRehash(user, parameter));

	ServerInstance->BuildISupport();

	return CMD_SUCCESS;
}

