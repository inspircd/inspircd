/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
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


/* $ModDesc: Provides the UNINVITE command which lets users un-invite other users from channels (!) */

#include "inspircd.h"

/** Handle /UNINVITE
 */
class CommandUninvite : public Command
{
 public:
	CommandUninvite (InspIRCd* Instance) : Command(Instance,"UNINVITE", 0, 2)
	{
		this->source = "m_uninvite.so";
		syntax = "<nick> <channel>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		User* u = ServerInstance->FindNick(parameters[0]);
		Channel* c = ServerInstance->FindChan(parameters[1]);

		if ((!c) || (!u))
		{
			if (!c)
			{
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[1].c_str());
			}
			else
			{
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
			}

			return CMD_FAILURE;
		}

		if (IS_LOCAL(user))
		{
			if (c->GetStatus(user) < STATUS_HOP)
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator", user->nick.c_str(), c->name.c_str(), c->GetStatus(u) == STATUS_HOP ? "" : "half-");
				return CMD_FAILURE;
			}
		}

		irc::string xname(c->name.c_str());

		if (!u->IsInvited(xname))
		{
			user->WriteNumeric(505, "%s %s %s :Is not invited to channel %s", user->nick.c_str(), u->nick.c_str(), c->name.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}
		if (!c->HasUser(user))
		{
			user->WriteNumeric(492, "%s %s :You're not on that channel!",user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		u->RemoveInvite(xname);
		user->WriteNumeric(494, "%s %s %s :Uninvited", user->nick.c_str(), c->name.c_str(), u->nick.c_str());
		u->WriteNumeric(493, "%s :You were uninvited from %s by %s", u->nick.c_str(), c->name.c_str(), user->nick.c_str());
		c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s uninvited %s.", c->name.c_str(), user->nick.c_str(), u->nick.c_str());

		return CMD_SUCCESS;
	}
};

class ModuleUninvite : public Module
{
	CommandUninvite *mycommand;

 public:

	ModuleUninvite(InspIRCd* Me) : Module(Me)
	{

		mycommand = new CommandUninvite(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleUninvite()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR | VF_COMMON, API_VERSION);
	}
};

MODULE_INIT(ModuleUninvite)

