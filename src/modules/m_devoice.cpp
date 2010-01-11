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
	CommandDevoice(Module* Creator) : Command(Creator,"DEVOICE", 1)
	{
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
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};

class ModuleDeVoice : public Module
{
	CommandDevoice cmd;
 public:
	ModuleDeVoice() : cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleDeVoice()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides voiced users with the ability to devoice themselves.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleDeVoice)
