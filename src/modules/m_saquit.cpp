/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */

/** Handle /SAQUIT
 */
class CommandSaquit : public Command
{
 public:
	CommandSaquit (InspIRCd* Instance) : Command(Instance, "SAQUIT", "o", 2, 2, false, 0)
	{
		this->source = "m_saquit.so";
		syntax = "<nick> <reason>";
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
			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[0].c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSaquit : public Module
{
	CommandSaquit*	mycommand;
 public:
	ModuleSaquit(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSaquit(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSaquit()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSaquit)
