/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/* $ModDesc: Provides support for the CHGIDENT command */

/** Handle /CHGIDENT
 */
class CommandChgident : public Command
{
 public:
	CommandChgident (InspIRCd* Instance) : Command(Instance,"CHGIDENT", "o", 2)
	{
		this->source = "m_chgident.so";
		syntax = "<nick> <newident>";
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
			user->WriteServ("NOTICE %s :*** CHGIDENT: Ident must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.IdentMax)
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Ident is too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (!ServerInstance->IsIdent(parameters[1].c_str()))
		{
			user->WriteServ("NOTICE %s :*** CHGIDENT: Invalid characters in ident", user->nick.c_str());
			return CMD_FAILURE;
		}

		dest->ChangeIdent(parameters[1].c_str());

		if (!ServerInstance->ULine(user->server))
			ServerInstance->SNO->WriteToSnoMask(IS_LOCAL(dest) ? 'a' : 'A', "%s used CHGIDENT to change %s's ident to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str());

		/* route it! */
		return CMD_SUCCESS;
	}
};


class ModuleChgIdent : public Module
{
	CommandChgident* mycommand;


public:
	ModuleChgIdent(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandChgident(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleChgIdent()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleChgIdent)

