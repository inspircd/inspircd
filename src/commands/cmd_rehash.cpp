/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/** Handle /REHASH. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandRehash : public Command
{
 public:
	/** Constructor for rehash.
	 */
	CommandRehash ( Module* parent) : Command(parent,"REHASH",0) { flags_needed = 'o'; Penalty = 2; syntax = "[<servermask>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandRehash::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string param = parameters.size() ? parameters[0] : "";

	FOREACH_MOD(I_OnPreRehash,OnPreRehash(user, param));

	if (param.empty())
	{
		// standard rehash of local server
	}
	else if (param.find_first_of("*.") != std::string::npos)
	{
		// rehash of servers by server name (with wildcard)
		if (!InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
		{
			// Doesn't match us. PreRehash is already done, nothing left to do
			return CMD_SUCCESS;
		}
	}
	else
	{
		// parameterized rehash

		// the leading "-" is optional; remove it if present.
		if (param[0] == '-')
			param = param.substr(1);

		FOREACH_MOD(I_OnModuleRehash,OnModuleRehash(user, param));
		return CMD_SUCCESS;
	}

	// Rehash for me. Try to start the rehash thread
	if (!ServerInstance->ConfigThread)
	{
		std::string m = user->nick + " is rehashing config file " + ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()) + " on " + ServerInstance->Config->ServerName;
		ServerInstance->SNO->WriteGlobalSno('a', m);

		if (IS_LOCAL(user))
			user->WriteNumeric(RPL_REHASHING, "%s %s :Rehashing",
				user->nick.c_str(),ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()));
		else
			ServerInstance->PI->SendUserNotice(user, std::string("*** Rehashing server ") +
				ServerConfig::CleanFilename(ServerInstance->ConfigFileName.c_str()));

		/* Don't do anything with the logs here -- logs are restarted
		 * after the config thread has completed.
		 */
		ServerInstance->RehashUsersAndChans();
		FOREACH_MOD(I_OnGarbageCollect, OnGarbageCollect());


		ServerInstance->ConfigThread = new ConfigReaderThread(user->uuid);
		ServerInstance->Threads->Start(ServerInstance->ConfigThread);
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
	}

	// Always return success so spanningtree forwards an incoming REHASH even if we failed
	return CMD_SUCCESS;
}


COMMAND_INIT(CommandRehash)
