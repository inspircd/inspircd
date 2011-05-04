/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for SANICK command */

/** Handle /SANICK
 */
class CommandSanick : public Command
{
 public:
	CommandSanick(Module* Creator) : Command(Creator,"SANICK", 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<nick> <new-nick>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (target && ServerInstance->ULine(target->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			if (!target)
			{
				user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}

			if (parameters[1].compare("0") && !ServerInstance->IsNick(parameters[1].c_str(), ServerInstance->Config->Limits.NickMax))
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}
		}

		/* Have we hit target's server yet? */
		if (target && IS_LOCAL(target))
		{
			std::string oldnick = target->nick;
			if (!parameters[1].compare("0") && target->ChangeNick(target->uuid, true))
			{
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used SANICK to change "+oldnick+" to their UID ("+target->uuid+")");
			}
			else if (target->ChangeNick(parameters[1], true))
			{
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used SANICK to change "+oldnick+" to "+parameters[1]);
			}
			else
			{
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" failed SANICK (from "+oldnick+" to "+parameters[1]+")");
			}
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


class ModuleSanick : public Module
{
	CommandSanick cmd;
 public:
	ModuleSanick() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSanick()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for SANICK command", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleSanick)
