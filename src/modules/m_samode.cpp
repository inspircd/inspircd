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

/* $ModDesc: Provides more advanced UnrealIRCd SAMODE command */

#include "inspircd.h"

/** Handle /SAMODE
 */
class CommandSamode : public Command
{
 public:
	CommandSamode (InspIRCd* Instance) : Command(Instance,"SAMODE", "o", 2, false, 0)
	{
		this->source = "m_samode.so";
		syntax = "<target> <modes> {<mode-parameters>}";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/*
		 * Handles an SAMODE request. Notifies all +s users.
	 	 */
		ServerInstance->SendMode(parameters, ServerInstance->FakeClient);

		if (ServerInstance->Modes->GetLastParse().length())
		{
			ServerInstance->SNO->WriteToSnoMask('A', std::string(user->nick) + " used SAMODE: " + ServerInstance->Modes->GetLastParse());

			std::string channel = parameters[0];
			ServerInstance->PI->SendMode(channel, ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());

			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid SAMODE sequence.", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSaMode : public Module
{
	CommandSamode*	mycommand;
 public:
	ModuleSaMode(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSamode(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSaMode()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSaMode)
