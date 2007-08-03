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

/*
 * DEVOICE module for InspIRCd
 *  Syntax: /DEVOICE <#chan>
 */

/* $ModDesc: Provides voiced users with the ability to devoice themselves. */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/** Handle /DEVOICE
 */
class cmd_devoice : public command_t
{
 public:
	cmd_devoice (InspIRCd* Instance) : command_t(Instance,"DEVOICE", 0, 1)
	{
		this->source = "m_devoice.so";
		syntax = "<channel>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		chanrec* c = ServerInstance->FindChan(parameters[0]);
		if (c && c->HasUser(user))
		{
			const char* modes[3];
			modes[0] = parameters[0];
			modes[1] = "-v";
			modes[2] = user->nick;

			userrec* n = new userrec(ServerInstance);
			n->SetFd(FD_MAGIC_NUMBER);
			ServerInstance->SendMode(modes,3,n);
			delete n;

			/* route it -- SendMode doesn't distribute over the whole network */
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};

class ModuleDeVoice : public Module
{
	cmd_devoice *mycommand;
 public:
	ModuleDeVoice(InspIRCd* Me) : Module(Me)
	{

		mycommand = new cmd_devoice(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	virtual ~ModuleDeVoice()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleDeVoice)
