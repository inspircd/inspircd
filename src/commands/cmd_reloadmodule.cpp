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

class CommandReloadmodule : public Command
{
 public:
	/** Constructor for reloadmodule.
	 */
	CommandReloadmodule (InspIRCd* Instance, Module* parent) : Command(Instance, parent, "RELOADMODULE","o",1) { syntax = "<modulename>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandReloadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (ServerInstance->Modules->Unload(parameters[0].c_str()))
	{
		ServerInstance->SNO->WriteToSnoMask('a', "RELOAD MODULE: %s unloaded %s",user->nick.c_str(), parameters[0].c_str());
		if (ServerInstance->Modules->Load(parameters[0].c_str()))
		{
			ServerInstance->SNO->WriteToSnoMask('a', "RELOAD MODULE: %s reloaded %s",user->nick.c_str(), parameters[0].c_str());
			user->WriteNumeric(975, "%s %s :Module successfully reloaded.",user->nick.c_str(), parameters[0].c_str());
			return CMD_SUCCESS;
		}
	}

	ServerInstance->SNO->WriteToSnoMask('a', "RELOAD MODULE: %s unsuccessfully reloaded %s",user->nick.c_str(), parameters[0].c_str());
	user->WriteNumeric(975, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandReloadmodule)
