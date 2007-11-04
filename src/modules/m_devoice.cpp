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

/** Handle /DEVOICE
 */
class CommandDevoice : public Command
{
 public:
	CommandDevoice (InspIRCd* Instance) : Command(Instance,"DEVOICE", 0, 1)
	{
		this->source = "m_devoice.so";
		syntax = "<channel>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (c && c->HasUser(user))
		{
			const char* modes[3];
			modes[0] = parameters[0];
			modes[1] = "-v";
			modes[2] = user->nick;

			ServerInstance->SendMode(modes, 3, ServerInstance->FakeClient);

			/* route it -- SendMode doesn't distribute over the whole network */
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};

class ModuleDeVoice : public Module
{
	CommandDevoice *mycommand;
 public:
	ModuleDeVoice(InspIRCd* Me) : Module(Me)
	{

		mycommand = new CommandDevoice(ServerInstance);
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
