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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for GLOBOPS and user mode +g */

/** Handle /GLOBOPS
 */
class cmd_globops : public command_t
{
 public:
	cmd_globops (InspIRCd* Instance) : command_t(Instance,"GLOBOPS",'o',1)
	{
		this->source = "m_globops.so";
		syntax = "<any-text>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
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
	cmd_globops* mycommand;
 public:
	ModuleGlobops(InspIRCd* Me)
		: Module(Me)
	{
		mycommand = new cmd_globops(ServerInstance);
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

	void Implements(char* List)
	{
	}
};

MODULE_INIT(ModuleGlobops)
