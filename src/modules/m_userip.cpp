/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides support for USERIP command */

/** Handle /USERIP
 */
class CommandUserip : public Command
{
 public:
	CommandUserip(Module* Creator) : Command(Creator,"USERIP", 1)
	{
		syntax = "<nick> [<nick> ...]";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string retbuf = "340 " + user->nick + " :";
		int nicks = 0;
		bool checked_privs = false;
		bool has_privs = false;

		for (int i = 0; i < (int)parameters.size(); i++)
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
							user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - You do not have the required operator privileges",user->nick.c_str());
					}

					if (!has_privs)
						continue;
				}

				retbuf = retbuf + u->nick + (IS_OPER(u) ? "*" : "") + "=";
				if (IS_AWAY(u))
					retbuf += "-";
				else
					retbuf += "+";
				retbuf += u->ident + "@" + u->GetIPString() + " ";
				nicks++;
			}
		}

		if (nicks != 0)
			user->WriteServ(retbuf);

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

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		output = output + " USERIP";
	}

	virtual ~ModuleUserIP()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for USERIP command",VF_VENDOR);
	}

};

MODULE_INIT(ModuleUserIP)

