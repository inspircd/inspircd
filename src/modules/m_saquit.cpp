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

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */

/** Handle /SAQUIT
 */
class CommandSaquit : public Command
{
 public:
	CommandSaquit(Module* Creator) : Command(Creator, "SAQUIT", 2, 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<nick> <reason>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			// Pass the command on, so the client's server can quit it properly.
			if (!IS_LOCAL(dest))
				return CMD_SUCCESS;
			
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used SAQUIT to make "+std::string(dest->nick)+" quit with a reason of "+parameters[1]);

			ServerInstance->Users->QuitUser(dest, parameters[1]);
			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleSaquit : public Module
{
	CommandSaquit cmd;
 public:
	ModuleSaquit() : cmd(this) {}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleSaquit()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for an SAQUIT command, exits user with a reason", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleSaquit)
