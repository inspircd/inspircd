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

/* $ModDesc: Provides a pointless /dalinfo command, demo module */

/** Handle /DALINFO
 */
class CommandDalinfo : public Command
{
 public:
	/* Command 'dalinfo', takes no parameters and needs no special modes */
	CommandDalinfo (InspIRCd* Instance) : Command(Instance,"DALINFO", 0, 0)
	{
		this->source = "m_testcommand.so";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		user->WriteServ("NOTICE %s :*** DALNet had nothing to do with it.", user->nick);
		return CMD_FAILURE;
	}
};

class ModuleTestCommand : public Module
{
	CommandDalinfo* newcommand;
 public:
	ModuleTestCommand(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		newcommand = new CommandDalinfo(ServerInstance);
		ServerInstance->AddCommand(newcommand);

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

