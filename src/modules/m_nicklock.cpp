/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/* $ModDesc: Provides the NICKLOCK command, allows an oper to change a users nick and lock them to it until they quit */

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

		if ((!target) || (target->registered != REG_ALL))
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (!ServerInstance->IsNick(parameters[1].c_str(), ServerInstance->Config->Limits.NickMax))
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}

			user->WriteServ("947 %s %s :Nickname now locked.", user->nick.c_str(), parameters[1].c_str());
		}

		/* If we made it this far, extend the user */
		if (IS_LOCAL(target))
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

		if (!target)
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(target))
		{
			if (locked.set(target, 0))
			{
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used NICKUNLOCK on "+target->nick);
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
		: locked("nick_locked", this), cmd1(this, locked), cmd2(this, locked)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd1);
		ServerInstance->Modules->AddService(cmd2);
		ServerInstance->Modules->AddService(locked);
		ServerInstance->Modules->Attach(I_OnUserPreNick, this);
	}

	~ModuleNickLock()
	{
	}

	Version GetVersion()
	{
		return Version("Provides the NICKLOCK command, allows an oper to change a users nick and lock them to it until they quit", VF_OPTCOMMON | VF_VENDOR);
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

	void Prioritize()
	{
		Module *nflood = ServerInstance->Modules->Find("m_nickflood.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreNick, PRIORITY_BEFORE, &nflood);
	}
};

MODULE_INIT(ModuleNickLock)
