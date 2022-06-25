/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/isupport.h"

class CommandUserip final
	: public Command
{
private:
	UserModeReference hideopermode;

public:
	CommandUserip(Module* Creator)
		: Command(Creator,"USERIP", 1)
		, hideopermode(Creator, "hideoper")
	{
		allow_empty_last_param = false;
		syntax = { "<nick> [<nick>]+" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		const bool has_privs = user->HasPrivPermission("users/auspex");

		std::string retbuf;

		size_t paramcount = std::min<size_t>(parameters.size(), 5);
		for (size_t i = 0; i < paramcount; ++i)
		{
			User *u = ServerInstance->Users.FindNick(parameters[i]);
			if ((u) && (u->registered == REG_ALL))
			{
				retbuf += u->nick;

				if (u->IsOper())
				{
					// XXX: +H hidden opers must not be shown as opers
					if ((u == user) || (has_privs) || (!u->IsModeSet(hideopermode)))
						retbuf += '*';
				}

				retbuf += '=';
				retbuf += (u->IsAway() ? '-' : '+');
				retbuf += u->ident;
				retbuf += '@';
				retbuf += (u == user || has_privs ? u->GetIPString() : "255.255.255.255");
				retbuf += ' ';
			}
		}

		user->WriteNumeric(RPL_USERIP, retbuf);

		return CmdResult::SUCCESS;
	}
};

class ModuleUserIP final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandUserip cmd;

public:
	ModuleUserIP()
		: Module(VF_VENDOR, "Adds the /USERIP command which allows users to find out the IP address of one or more connected users.")
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["USERIP"];
	}
};

MODULE_INIT(ModuleUserIP)
