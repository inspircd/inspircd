/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	CommandGlobops (InspIRCd* Instance) : Command(Instance,"GLOBOPS",'o',1)
	{
		this->source = "m_globops.so";
		syntax = "<any-text>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		std::string line = "From " + std::string(user->nick) + ": ";
		for (int i = 0; i < pcnt; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		ServerInstance->SNO->WriteToSnoMask('g',line);

		/* route it (ofc :p) */
		return CMD_SUCCESS;
	}
};

class ModuleGlobops : public Module
{
	CommandGlobops* mycommand;
 public:
	ModuleGlobops(InspIRCd* Me)
		: Module(Me)
	{
		mycommand = new CommandGlobops(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		ServerInstance->SNO->EnableSnomask('g',"GLOBOPS");

	}
	
	virtual ~ModuleGlobops()
	{
		ServerInstance->SNO->DisableSnomask('g');
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 1, VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleGlobops)
