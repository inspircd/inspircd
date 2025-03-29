/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "extension.h"
#include "numerichelper.h"

enum
{
	// InspIRCd-specific.
	ERR_NICKNOTLOCKED = 946,
	RPL_NICKLOCKON = 947,
	RPL_NICKLOCKOFF = 945
};

class CommandNicklock final
	: public Command
{
public:
	BoolExtItem& locked;
	CommandNicklock (Module* Creator, BoolExtItem& ext)
		: Command(Creator, "NICKLOCK", 2)
		, locked(ext)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> <newnick>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* target = ServerInstance->Users.Find(parameters[0], true);
		if (!target)
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (!ServerInstance->Users.IsNick(parameters[1]))
			{
				user->WriteNotice("*** Invalid nickname '" + parameters[1] + "'");
				return CmdResult::FAILURE;
			}

			user->WriteNumeric(RPL_NICKLOCKON, parameters[1], "Nickname now locked.");
		}

		/* If we made it this far, extend the user */
		if (IS_LOCAL(target))
		{
			locked.Set(target);

			std::string oldnick = target->nick;
			if (target->ChangeNick(parameters[1]))
				ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used NICKLOCK to change and hold "+oldnick+" to "+parameters[1]);
			else
			{
				std::string newnick = target->nick;
				ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used NICKLOCK, but "+oldnick+" failed nick change to "+parameters[1]+" and was locked to "+newnick+" instead");
			}
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class CommandNickunlock final
	: public Command
{
public:
	BoolExtItem& locked;
	CommandNickunlock (Module* Creator, BoolExtItem& ext)
		: Command(Creator, "NICKUNLOCK", 1)
		, locked(ext)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick>" };
		translation = { TR_NICK };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* target = ServerInstance->Users.Find(parameters[0]);

		if (!target)
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		if (IS_LOCAL(target))
		{
			if (locked.Get(target))
			{
				locked.Unset(target);
				ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used NICKUNLOCK on "+target->nick);
				user->WriteRemoteNumeric(RPL_NICKLOCKOFF, target->nick, "Nickname now unlocked.");
			}
			else
			{
				user->WriteRemoteNumeric(ERR_NICKNOTLOCKED, target->nick, "This user's nickname is not locked.");
				return CmdResult::FAILURE;
			}
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleNickLock final
	: public Module
{
private:
	BoolExtItem locked;
	CommandNicklock cmd1;
	CommandNickunlock cmd2;

public:
	ModuleNickLock()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /NICKLOCK command which allows server operators to change a user's nickname and prevent them from changing it again until they disconnect.")
		, locked(this, "nick-locked", ExtensionType::USER)
		, cmd1(this, locked)
		, cmd2(this, locked)
	{
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		if (locked.Get(user))
		{
			user->WriteNumeric(ERR_CANTCHANGENICK, "You cannot change your nickname (your nick is locked)");
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void Prioritize() override
	{
		Module* nflood = ServerInstance->Modules.Find("nickflood");
		ServerInstance->Modules.SetPriority(this, I_OnUserPreNick, PRIORITY_BEFORE, nflood);
	}
};

MODULE_INIT(ModuleNickLock)
