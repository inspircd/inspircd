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

/* $ModDesc: Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit */

/** Handle /NICKLOCK
 */
class CommandNicklock : public Command
{
 public:
	LocalIntExt& locked;
	CommandNicklock (Module* Creator, LocalIntExt& ext) : Command(Creator,"NICKLOCK", 1),
		locked(ext)
	{
		flags_needed = 'o';
		syntax = "<oldnick> [newnick]";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		if (!target)
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (parameters.size() > 1 && parameters[1].compare("0") && !ServerInstance->IsNick(parameters[1].c_str(), ServerInstance->Config->Limits.NickMax))
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}

			user->WriteServ("947 %s %s :Nickname now locked.", user->nick.c_str(), parameters.size() > 1 ? parameters[1].c_str() : parameters[0].c_str());
		}

		/* If we made it this far, extend the user */
		if (IS_LOCAL(target))
		{
			locked.set(target, 1);

			std::string oldnick = target->nick;
			if (parameters.size() < 2)
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKLOCK to lock the nick of "+oldnick);
			else if (!parameters[1].compare("0") && target->ForceNickChange(target->uuid))
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKLOCK to change and lock the nick of "+oldnick+" to their UID ("+target->uuid+")");
			else if (target->ForceNickChange(parameters[1].c_str()))
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKLOCK to change and lock the nick of "+oldnick+" to "+parameters[1]);
			else
			{
				std::string newnick = target->nick;
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKLOCK, but "+oldnick+" failed nick change to "+parameters[1]+" and was locked to "+newnick+" instead");
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

/** Handle /NICKUNLOCK
 */
class CommandNickunlock : public Command
{
 public:
	LocalIntExt& locked;
	CommandNickunlock (Module* Creator, LocalIntExt& ext) : Command(Creator,"NICKUNLOCK", 1),
		locked(ext)
	{
		flags_needed = 'o';
		syntax = "<locked-nick>";
		TRANSLATE2(TR_NICK, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		if (!target)
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(target))
		{
			if (locked.set(target, 0))
			{
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKUNLOCK to unlock the nick of "+target->nick);
				user->SendText(":%s 945 %s %s :Nickname now unlocked.",
					ServerInstance->Config->ServerName.c_str(),user->nick.c_str(),target->nick.c_str());
			}
			else
			{
				user->SendText(":%s 946 %s %s :This user's nickname is not locked.",
					ServerInstance->Config->ServerName.c_str(),user->nick.c_str(),target->nick.c_str());
				return CMD_FAILURE;
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


class ModuleNickLock : public Module
{
	LocalIntExt locked;
	CommandNicklock cmd1;
	CommandNickunlock cmd2;
 public:
	ModuleNickLock()
		: locked(EXTENSIBLE_USER, "nick_locked", this), cmd1(this, locked), cmd2(this, locked)
	{}

	void init()
	{
		ServerInstance->AddCommand(&cmd1);
		ServerInstance->AddCommand(&cmd2);
		ServerInstance->Extensions.Register(&locked);
		ServerInstance->Modules->Attach(I_OnUserPreNick, this);
	}

	~ModuleNickLock()
	{
	}

	Version GetVersion()
	{
		return Version("Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit", VF_OPTCOMMON | VF_VENDOR);
	}

	ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->NICKForced.get(user)) /* Allow forced nick changes */
			return MOD_RES_PASSTHRU;

		if (locked.get(user))
		{
			user->WriteNumeric(447, "%s :You cannot change your nickname (your nick is locked)",user->nick.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNickLock)
