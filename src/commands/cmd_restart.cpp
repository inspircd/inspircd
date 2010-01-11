/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /RESTART
 */
class CommandRestart : public Command
{
 public:
	/** Constructor for restart.
	 */
	CommandRestart(Module* parent) : Command(parent,"RESTART",1,1) { flags_needed = 'o'; syntax = "<password>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandRestart::Handle (const std::vector<std::string>& parameters, User *user)
{
	ServerInstance->Logs->Log("COMMAND",DEFAULT,"Restart: %s",user->nick.c_str());
	if (!ServerInstance->PassCompare(user, ServerInstance->Config->restartpass, parameters[0].c_str(), ServerInstance->Config->powerhash))
	{
		ServerInstance->SNO->WriteGlobalSno('a', "RESTART command from %s!%s@%s, restarting server.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());

		try
		{
			ServerInstance->Restart("Server restarting.");
		}
		catch (...)
		{
			/* We dont actually get here unless theres some fatal and unrecoverable error. */
			exit(0);
		}
	}
	else
	{
		ServerInstance->SNO->WriteGlobalSno('a', "Failed RESTART Command from %s!%s@%s.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandRestart)
