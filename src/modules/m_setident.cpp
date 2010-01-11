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

#include "inspircd.h"

/* $ModDesc: Provides support for the SETIDENT command */

/** Handle /SETIDENT
 */
class CommandSetident : public Command
{
 public:
 CommandSetident(Module* Creator) : Command(Creator,"SETIDENT", 1)
	{
		flags_needed = 'o'; syntax = "<new-ident>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		if (parameters.size() == 0)
		{
			user->WriteServ("NOTICE %s :*** SETIDENT: Ident must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[0].size() > ServerInstance->Config->Limits.IdentMax)
		{
			user->WriteServ("NOTICE %s :*** SETIDENT: Ident is too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (!ServerInstance->IsIdent(parameters[0].c_str()))
		{
			user->WriteServ("NOTICE %s :*** SETIDENT: Invalid characters in ident", user->nick.c_str());
			return CMD_FAILURE;
		}

		user->ChangeIdent(parameters[0].c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "%s used SETIDENT to change their ident to '%s'", user->nick.c_str(), user->ident.c_str());

		return CMD_SUCCESS;
	}
};


class ModuleSetIdent : public Module
{
	CommandSetident cmd;

 public:
	ModuleSetIdent() : cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSetIdent()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for the SETIDENT command", VF_VENDOR);
	}

};


MODULE_INIT(ModuleSetIdent)
