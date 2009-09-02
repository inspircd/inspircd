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

// Globops and +g support module by C.J.Edwards

#include "inspircd.h"

/* $ModDesc: Provides support for GLOBOPS and user mode +g */

/** Handle /GLOBOPS
 */
class CommandGlobops : public Command
{
 public:
	CommandGlobops (InspIRCd* Instance) : Command(Instance,"GLOBOPS","o",1,1)
	{
		this->source = "m_globops.so";
		syntax = "<any-text>";
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
};

class ModuleGlobops : public Module
{
	CommandGlobops cmd;
 public:
	ModuleGlobops(InspIRCd* Me)
		: Module(Me), cmd(Me)
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
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleGlobops)
