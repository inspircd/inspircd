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

// Globops and +g support module by C.J.Edwards

#include "inspircd.h"

/* $ModDesc: Provides support for GLOBOPS and user mode +g */

/** Handle /GLOBOPS
 */
class CommandGlobops : public Command
{
 public:
	CommandGlobops(Module* Creator) : Command(Creator,"GLOBOPS", 1,1)
	{
		flags_needed = 'o'; syntax = "<any-text>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string line = "From " + std::string(user->nick) + ": ";
		for (int i = 0; i < (int)parameters.size(); i++)
		{
			line = line + parameters[i] + " ";
		}
		ServerInstance->SNO->WriteToSnoMask('g',line);

		/* route it (ofc :p) */
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleGlobops : public Module
{
	CommandGlobops cmd;
 public:
	ModuleGlobops()
		: cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
		ServerInstance->SNO->EnableSnomask('g',"GLOBOPS");

	}

	virtual ~ModuleGlobops()
	{
		ServerInstance->SNO->DisableSnomask('g');
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for GLOBOPS and user mode +g", VF_COMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleGlobops)
