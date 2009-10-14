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

/* $ModDesc: Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit */

/** Handle /NICKLOCK
 */
class CommandNicklock : public Command
{
 public:
	LocalIntExt& locked;
	CommandNicklock (Module* Creator, LocalIntExt& ext) : Command(Creator,"NICKLOCK", 2),
		locked(ext)
	{
		flags_needed = 'o';
		syntax = "<oldnick> <newnick>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (target && ServerInstance->ULine(target->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an NICKLOCK command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			if (!target)
			{
				user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}

			if (!ServerInstance->IsNick(parameters[1].c_str(), ServerInstance->Config->Limits.NickMax))
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}

			user->WriteServ("947 %s %s :Nickname now locked.", user->nick.c_str(), parameters[1].c_str());
		}

		/* If we made it this far, extend the user */
		if (target && IS_LOCAL(target))
		{
			locked.set(target, 1);

			std::string oldnick = target->nick;
			if (target->ForceNickChange(parameters[1].c_str()))
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKLOCK to change and hold "+oldnick+" to "+parameters[1]);
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

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (target && ServerInstance->ULine(target->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an NICKUNLOCK command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			if (!target)
			{
				user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}

		if (target && IS_LOCAL(target))
		{
			if (locked.set(target, 0))
			{
				ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used NICKUNLOCK on "+parameters[0]);
				user->WriteNumeric(945, "%s %s :Nickname now unlocked.",user->nick.c_str(),target->nick.c_str());
			}
			else
			{
				user->WriteNumeric(946, "%s %s :This user's nickname is not locked.",user->nick.c_str(),target->nick.c_str());
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
		: locked("nick_locked", this), cmd1(this, locked), cmd2(this, locked)
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
		return Version("Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit", VF_COMMON | VF_VENDOR, API_VERSION);
	}


	ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		if (isdigit(newnick[0])) /* Allow a switch to a UID */
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

	void Prioritize()
	{
		Module *nflood = ServerInstance->Modules->Find("m_nickflood.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_BEFORE, &nflood);
	}
};

MODULE_INIT(ModuleNickLock)
