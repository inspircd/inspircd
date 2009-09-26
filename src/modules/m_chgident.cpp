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
	CommandChgident(Module* Creator) : Command(Creator,"CHGIDENT", 2)
	{
		flags_needed = 'o'; syntax = "<nick> <newident>";
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

		if (IS_LOCAL(dest))
		{
			dest->ChangeIdent(parameters[1].c_str());

			if (!ServerInstance->ULine(user->server))
				ServerInstance->SNO->WriteGlobalSno('a', "%s used CHGIDENT to change %s's ident to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str());
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};


class ModuleChgIdent : public Module
{
	CommandChgident cmd;

public:
	ModuleChgIdent() : cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleChgIdent()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for the CHGIDENT command", VF_OPTCOMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleChgIdent)

