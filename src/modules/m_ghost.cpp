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
#include "account.h"

/* $ModDesc: Allow users to kill their own ghost sessions */

static dynamic_reference<AccountProvider> accounts("account");

/** Handle /GHOST
 */
class CommandGhost : public Command
{
 public:
	CommandGhost(Module* Creator) : Command(Creator,"GHOST", 1, 1)
	{
		syntax = "<nick>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		if(!target)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		if(IS_LOCAL(user))
		{
			if(!accounts || !accounts->IsRegistered(user))
			{
				user->WriteServ("NOTICE %s :You are not logged in", user->nick.c_str());
				return CMD_FAILURE;
			}
			if(accounts->GetAccountName(user) != accounts->GetAccountName(target))
			{
				user->WriteServ("NOTICE %s :They are not logged in as you", user->nick.c_str());
				return CMD_FAILURE;
			}
			if(user == target)
			{
				user->WriteServ("NOTICE %s :You may not ghost yourself", user->nick.c_str());
				return CMD_FAILURE;
			}
			user->WriteServ("NOTICE %s :User %s ghosted successfully", user->nick.c_str(), parameters[0].c_str());
		}
		if(IS_LOCAL(target))
			ServerInstance->Users->QuitUser(target, "GHOST command used by " + user->nick);
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

class ModuleGhost : public Module
{
	CommandGhost cmd_ghost;

 public:
	ModuleGhost() : cmd_ghost(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd_ghost);
	}

	Version GetVersion()
	{
		return Version("Allow users to kill their own ghost sessions", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleGhost)
