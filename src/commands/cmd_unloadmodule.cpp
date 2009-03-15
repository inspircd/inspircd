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
#include "commands/cmd_unloadmodule.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandUnloadmodule(Instance);
}

CmdResult CommandUnloadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (ServerInstance->Modules->Unload(parameters[0].c_str()))
	{
		ServerInstance->SNO->WriteToSnoMask('A', "MODULE UNLOADED: %s unloaded %s", user->nick.c_str(), parameters[0].c_str());
		user->WriteNumeric(973, "%s %s :Module successfully unloaded.",user->nick.c_str(), parameters[0].c_str());
	}
	else
	{
		user->WriteNumeric(972, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
