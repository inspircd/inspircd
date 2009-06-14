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

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (c && c->HasUser(user))
		{
			std::vector<std::string> modes;
			modes.push_back(parameters[0]);
			modes.push_back("-v");
			modes.push_back(user->nick);

			ServerInstance->SendMode(modes, ServerInstance->FakeClient);
			ServerInstance->PI->SendMode(c->name, ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());
			return CMD_LOCALONLY;
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
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleDeVoice)
