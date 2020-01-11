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
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

/** Handle /USERIP
 */
class CommandUserip : public Command
{
 public:
	CommandUserip(Module* Creator) : Command(Creator,"USERIP", 1)
	{
		syntax = "<nick> [<nick>]+";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		std::string retbuf;
		int nicks = 0;
		bool checked_privs = false;
		bool has_privs = false;

		for (size_t i = 0; i < parameters.size(); i++)
		{
			User *u = ServerInstance->FindNickOnly(parameters[i]);
			if ((u) && (u->registered == REG_ALL))
			{
				// Anyone may query their own IP
				if (u != user)
				{
					if (!checked_privs)
					{
						// Do not trigger the insufficient priviliges message more than once
						checked_privs = true;
						has_privs = user->HasPrivPermission("users/auspex");
						if (!has_privs)
							user->WriteNumeric(ERR_NOPRIVILEGES, "Permission Denied - You do not have the required operator privileges");
					}

					if (!has_privs)
						continue;
				}

				retbuf = retbuf + u->nick + (u->IsOper() ? "*" : "") + "=";
				if (u->IsAway())
					retbuf += "-";
				else
					retbuf += "+";
				retbuf += u->ident + "@" + u->GetIPString() + " ";
				nicks++;
			}
		}

		if (nicks != 0)
			user->WriteNumeric(RPL_USERIP, retbuf);

		return CMD_SUCCESS;
	}
};

class ModuleUserIP : public Module
{
	CommandUserip cmd;
 public:
	ModuleUserIP()
		: cmd(this)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["USERIP"];
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the USERIP command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleUserIP)
