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
		if (!InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
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

	if (IS_LOCAL(user))
		user->WriteNumeric(RPL_REHASHING, "%s %s :Rehashing",user->nick.c_str(),ServerConfig::CleanFilename(ServerInstance->ConfigFileName));
	else
		ServerInstance->PI->SendUserNotice(user, std::string("*** Rehashing server ") + ServerInstance->ConfigFileName);


	std::string m = user->nick + " is rehashing config file " + ServerConfig::CleanFilename(ServerInstance->ConfigFileName) + " on " + ServerInstance->Config->ServerName;
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
		ServerInstance->Config->RehashUserUID = user->uuid;
		ServerInstance->Config->RehashParameter = parameters.size() ? parameters[0] : "";

		ServerInstance->ConfigThread = new ConfigReaderThread(ServerInstance, false, ServerInstance->Config->RehashUserUID);
		ServerInstance->Threads->Create(ServerInstance->ConfigThread);
	}
	else
	{
		/*
		 * A rehash is already in progress! ahh shit.
		 * XXX, todo: we should find some way to kill runaway rehashes that are blocking, this is a major problem for unrealircd users
		 */
		if (IS_LOCAL(user))
			user->WriteServ("NOTICE %s :*** Could not rehash: A rehash is already in progress.", user->nick.c_str());
		else
			ServerInstance->PI->SendUserNotice(user, "*** Could not rehash: A rehash is already in progress.");

		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

