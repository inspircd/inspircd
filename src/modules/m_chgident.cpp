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

#include "inspircd.h"

/* $ModDesc: Provides support for the CHGIDENT command */

/** Handle /CHGIDENT
 */
class CommandChgident : public Command
{
 public:
	CommandChgident (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"CHGIDENT", "o", 2)
	{
		syntax = "<nick> <newident>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].empty())
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Ident must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.IdentMax)
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Ident is too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (!ServerInstance->IsIdent(parameters[1].c_str()))
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Invalid characters in ident", user->nick.c_str());
			return CMD_FAILURE;
		}

		dest->ChangeIdent(parameters[1].c_str());

		if (!ServerInstance->ULine(user->server))
			ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(dest) ? 'a' : 'A', "%s used CHGIDENT to change %s's ident to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str());

		/* route it! */
		return CMD_SUCCESS;
	}
};


class ModuleChgIdent : public Module
{
	CommandChgident cmd;

public:
	ModuleChgIdent(InspIRCd* Me) : Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleChgIdent()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleChgIdent)

