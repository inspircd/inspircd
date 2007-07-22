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

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides a pointless /dalinfo command, demo module */

/** Handle /DALINFO
 */
class cmd_dalinfo : public command_t
{
 public:
	/* Command 'dalinfo', takes no parameters and needs no special modes */
	cmd_dalinfo (InspIRCd* Instance) : command_t(Instance,"DALINFO", 0, 0)
	{
		this->source = "m_testcommand.so";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		user->WriteServ("NOTICE %s :*** DALNet had nothing to do with it.", user->nick);
		return CMD_FAILURE;
	}
};

class ModuleTestCommand : public Module
{
	cmd_dalinfo* newcommand;
 public:
	ModuleTestCommand(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		newcommand = new cmd_dalinfo(ServerInstance);
		ServerInstance->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
	}

	virtual ~ModuleTestCommand()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleTestCommand)

