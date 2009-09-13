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

#ifndef __CMD_DIE_H__
#define __CMD_DIE_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /DIE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandDie : public Command
{
 public:
	/** Constructor for die.
	 */
	CommandDie ( Module* parent) : Command(parent,"DIE",1) { flags_needed = 'o'; syntax = "<password>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif

#include "exitcodes.h"

/** Handle /DIE
 */
CmdResult CommandDie::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (!ServerInstance->PassCompare(user, ServerInstance->Config->diepass, parameters[0].c_str(), ServerInstance->Config->powerhash))
	{
		{
			std::string diebuf = std::string("*** DIE command from ") + user->nick + "!" + user->ident + "@" + user->dhost + ". Terminating in " + ConvToStr(ServerInstance->Config->DieDelay) + " seconds.";
			ServerInstance->Logs->Log("COMMAND",SPARSE, diebuf);
			ServerInstance->SendError(diebuf);
		}

		if (ServerInstance->Config->DieDelay)
			sleep(ServerInstance->Config->DieDelay);

		ServerInstance->Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Logs->Log("COMMAND",SPARSE, "Failed /DIE command from %s!%s@%s", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Failed DIE Command from %s!%s@%s.",user->nick.c_str(),user->ident.c_str(),user->host.c_str());
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandDie)
