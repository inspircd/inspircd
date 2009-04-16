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
#include "commands/cmd_loadmodule.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandLoadmodule(Instance);
}

/** Handle /LOADMODULE
 */
CmdResult CommandLoadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (ServerInstance->Modules->Load(parameters[0].c_str()))
	{
		ServerInstance->SNO->WriteToSnoMask('a', "NEW MODULE: %s loaded %s",user->nick.c_str(), parameters[0].c_str());
		user->WriteNumeric(975, "%s %s :Module successfully loaded.",user->nick.c_str(), parameters[0].c_str());
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteNumeric(974, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
		return CMD_FAILURE;
	}
}
