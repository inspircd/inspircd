/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /LOADMODULE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandLoadmodule : public Command
{
 public:
	/** Constructor for loadmodule.
	 */
	CommandLoadmodule ( Module* parent) : Command(parent,"LOADMODULE",1,1) { flags_needed='o'; syntax = "<modulename>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};


/** Handle /LOADMODULE
 */
CmdResult CommandLoadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (ServerInstance->Modules->Load(parameters[0].c_str()))
	{
		ServerInstance->SNO->WriteGlobalSno('a', "NEW MODULE: %s loaded %s",user->nick.c_str(), parameters[0].c_str());
		user->WriteNumeric(975, "%s %s :Module successfully loaded.",user->nick.c_str(), parameters[0].c_str());
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteNumeric(974, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
		return CMD_FAILURE;
	}
}

COMMAND_INIT(CommandLoadmodule)
