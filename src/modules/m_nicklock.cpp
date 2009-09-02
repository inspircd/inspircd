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
	CommandNicklock (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"NICKLOCK", "o", 2)
	{
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
			// This has to be done *here*, because this metadata must be stored netwide.
			target->Extend("nick_locked", "ON");

			/* Only send out nick from local server */
			target->Extend("nick_locked");

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
	CommandNickunlock (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"NICKUNLOCK", "o", 1)
	{
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
			if (target->Shrink("nick_locked"))
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
	CommandNicklock cmd1;
	CommandNickunlock cmd2;
 public:
	ModuleNickLock(InspIRCd* Me)
		: Module(Me), cmd1(Me, this), cmd2(Me, this)
	{
		ServerInstance->AddCommand(&cmd1);
		ServerInstance->AddCommand(&cmd2);
		Implementation eventlist[] = { I_OnUserPreNick, I_OnUserQuit, I_OnCleanup };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual ~ModuleNickLock()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}


	virtual int OnUserPreNick(User* user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return 0;

		if (isdigit(newnick[0])) /* Allow a switch to a UID */
			return 0;

		if (user->GetExt("NICKForced")) /* Allow forced nick changes */
			return 0;

		if (user->GetExt("nick_locked"))
		{
			user->WriteNumeric(447, "%s :You cannot change your nickname (your nick is locked)",user->nick.c_str());
			return 1;
		}
		return 0;
	}

	virtual void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message)
	{
		user->Shrink("nick_locked");
	}

	void Prioritize()
	{
		Module *nflood = ServerInstance->Modules->Find("m_nickflood.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_BEFORE, &nflood);
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			User* user = (User*)item;
			user->Shrink("nick_locked");
		}
	}
};

MODULE_INIT(ModuleNickLock)
