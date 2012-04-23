/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides support for the CHGNAME command */

/** Handle /CHGNAME
 */
class CommandChgname : public Command
{
 public:
	CommandChgname (InspIRCd* Instance) : Command(Instance,"CHGNAME", "o", 2, 2)
	{
		this->source = "m_chgname.so";
		syntax = "<nick> <newname>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].empty())
		{
			user->WriteServ("NOTICE %s :*** GECOS must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.MaxGecos)
		{
			user->WriteServ("NOTICE %s :*** GECOS too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeName(parameters[1].c_str());
			ServerInstance->SNO->WriteGlobalSno('a', "%s used CHGNAME to change %s's real name to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->fullname.c_str());
		}

		/* route it! */
		return CMD_SUCCESS;
	}
};


class ModuleChgName : public Module
{
	CommandChgname* mycommand;


public:
	ModuleChgName(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandChgname(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleChgName()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleChgName)
