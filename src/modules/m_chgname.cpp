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

/* $ModDesc: Provides support for the CHGNAME command */

/** Handle /CHGNAME
 */
class CommandChgname : public Command
{
 public:
	CommandChgname(Module* Creator) : Command(Creator,"CHGNAME", 2, 2)
	{
		flags_needed = 'o';
		syntax = "<nick> <newname>";
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

		if (parameters[1].length() > ServerInstance->Config->Limits.MaxGecos)
		{
			user->WriteServ("NOTICE %s :*** GECOS too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeName(parameters[1].c_str());
			ServerInstance->SNO->WriteGlobalSno('a', "%s used CHGNAME to change %s's real name to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->fullname.c_str());
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


class ModuleChgName : public Module
{
	CommandChgname cmd;

public:
	ModuleChgName() : cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleChgName()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for the CHGNAME command", VF_OPTCOMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleChgName)
