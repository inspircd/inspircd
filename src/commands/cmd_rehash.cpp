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
	user->WriteNumeric(382, "%s %s :Rehashing",user->nick,ServerConfig::CleanFilename(ServerInstance->ConfigFileName));
	std::string old_disabled = ServerInstance->Config->DisabledCommands;

	ServerInstance->Logs->Log("fuckingrehash", DEBUG, "parc %d p0 %s", pcnt, parameters[0]);
	if (pcnt && parameters[0][0] != '-')
	{
		if (!ServerInstance->MatchText(ServerInstance->Config->ServerName, parameters[0]))
		{
			ServerInstance->Logs->Log("fuckingrehash", DEBUG, "rehash for a server, and not for us");
			FOREACH_MOD(I_OnRehash,OnRehash(user, parameters[0]));
			return CMD_SUCCESS; // rehash for a server, and not for us
		}
	}
	else if (pcnt)
	{
		ServerInstance->Logs->Log("fuckingrehash", DEBUG, "rehash for a subsystem, ignoring");
		FOREACH_MOD(I_OnRehash,OnRehash(user, parameters[0]));
		return CMD_SUCCESS;
	}

	// Rehash for me.
	FOREACH_MOD(I_OnRehash,OnRehash(user, ""));

	std::string m = std::string(user->nick) + " is rehashing config file " + ServerConfig::CleanFilename(ServerInstance->ConfigFileName);
	ServerInstance->SNO->WriteToSnoMask('A', m);
	ServerInstance->PI->SendSNONotice("A", m);
	ServerInstance->Logs->CloseLogs();

	if (!ServerInstance->OpenLog(ServerInstance->Config->argv, ServerInstance->Config->argc))
	{
		m = std::string("ERROR: Could not open logfile ") + ServerInstance->Config->logpath + ":" + strerror(errno);
		ServerInstance->SNO->WriteToSnoMask('A', m);
		ServerInstance->PI->SendSNONotice("A", m);
	}

	ServerInstance->RehashUsersAndChans();
	FOREACH_MOD(I_OnGarbageCollect, OnGarbageCollect());

	if (!ServerInstance->ConfigThread)
	{
		ServerInstance->Config->RehashUser = user;
		ServerInstance->Config->RehashParameter = pcnt ? parameters[0] : "";

		ServerInstance->ConfigThread = new ConfigReaderThread(ServerInstance, false, user);
		ServerInstance->Threads->Create(ServerInstance->ConfigThread);
	}
	else
	{
		/* A rehash is already in progress! ahh shit. */
		if (IS_LOCAL(user))
			user->WriteServ("NOTICE %s :*** Could not rehash: A rehash is already in progress.", user->nick);
		else
			ServerInstance->PI->SendUserNotice(user, "*** Could not rehash: A rehash is already in progress.");

		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

