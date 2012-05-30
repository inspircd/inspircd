/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004-2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides support for /KNOCK and channel mode +K */

/** Handles the /KNOCK command
 */
class CommandKnock : public Command
{
 public:
	CommandKnock (InspIRCd* Instance) : Command(Instance,"KNOCK", 0, 2)
	{
		this->source = "m_knock.so";
		syntax = "<channel> <reason>";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		std::string line;

		if (!c)
		{
			user->WriteNumeric(401, "%s %s :No such channel",user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (c->HasUser(user))
		{
			user->WriteNumeric(480, "%s :Can't KNOCK on %s, you are already on that channel.", user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		if (c->IsModeSet('K'))
		{
			user->WriteNumeric(480, "%s :Can't KNOCK on %s, +K is set.",user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		if (!c->modes[CM_INVITEONLY])
		{
			user->WriteNumeric(480, "%s :Can't KNOCK on %s, channel is not invite only so knocking is pointless!",user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		for (int i = 1; i < (int)parameters.size() - 1; i++)
		{
			line = line + parameters[i] + " ";
		}
		line = line + parameters[parameters.size()-1];

		c->WriteChannelWithServ((char*)ServerInstance->Config->ServerName,  "NOTICE %s :User %s is KNOCKing on %s (%s)", c->name.c_str(), user->nick.c_str(), c->name.c_str(), line.c_str());
		user->WriteServ("NOTICE %s :KNOCKing on %s", user->nick.c_str(), c->name.c_str());
		return CMD_SUCCESS;
	}
};

/** Handles channel mode +K
 */
class Knock : public SimpleChannelModeHandler
{
 public:
	Knock(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'K') { }
};

class ModuleKnock : public Module
{
	CommandKnock* mycommand;
	Knock* kn;
 public:
	ModuleKnock(InspIRCd* Me) : Module(Me)
	{
		kn = new Knock(ServerInstance);

		if (!ServerInstance->Modes->AddMode(kn))
			throw ModuleException("Could not add new modes!");

		mycommand = new CommandKnock(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}


	virtual ~ModuleKnock()
	{
		ServerInstance->Modes->DelMode(kn);
		delete kn;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleKnock)
