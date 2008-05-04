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

CmdResult CommandRehash::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string old_disabled = ServerInstance->Config->DisabledCommands;

	if (parameters.size() && parameters[0][0] != '-')
	{
		if (!ServerInstance->MatchText(ServerInstance->Config->ServerName, parameters[0]))
		{
			FOREACH_MOD(I_OnRehash,OnRehash(user, parameters[0]));
			return CMD_SUCCESS; // rehash for a server, and not for us
		}
	}
	else if (parameters.size())
	{
		FOREACH_MOD(I_OnRehash,OnRehash(user, parameters[0]));
		return CMD_SUCCESS;
	}

	// Rehash for me.
	FOREACH_MOD(I_OnRehash,OnRehash(user, ""));

	// XXX write this to a remote user correctly
	user->WriteNumeric(382, "%s %s :Rehashing",user->nick,ServerConfig::CleanFilename(ServerInstance->ConfigFileName));

	std::string m = std::string(user->nick) + " is rehashing config file " + ServerConfig::CleanFilename(ServerInstance->ConfigFileName) + " on " + ServerInstance->Config->ServerName;
	ServerInstance->SNO->WriteToSnoMask('A', m);
	ServerInstance->Logs->CloseLogs();

	if (!ServerInstance->OpenLog(ServerInstance->Config->argv, ServerInstance->Config->argc))
	{
		m = std::string("ERROR: Could not open logfile ") + ServerInstance->Config->logpath + ":" + strerror(errno);
		ServerInstance->SNO->WriteToSnoMask('A', m);
	}

	ServerInstance->RehashUsersAndChans();
	FOREACH_MOD(I_OnGarbageCollect, OnGarbageCollect());

	if (!ServerInstance->ConfigThread)
	{
		ServerInstance->Config->RehashUser = user;
		ServerInstance->Config->RehashParameter = parameters.size() ? parameters[0] : "";

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

