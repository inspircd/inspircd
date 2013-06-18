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

/** Handles the /KNOCK command
 */
class CommandKnock : public Command
{
	SimpleChannelModeHandler& noknockmode;
	ChanModeReference inviteonlymode;

 public:
	bool sendnotice;
	bool sendnumeric;
	CommandKnock(Module* Creator, SimpleChannelModeHandler& Noknockmode)
		: Command(Creator,"KNOCK", 2, 2)
		, noknockmode(Noknockmode)
		, inviteonlymode(Creator, "inviteonly")
	{
		syntax = "<channel> <reason>";
		Penalty = 5;
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
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

		if (c->IsModeSet(noknockmode))
		{
			user->WriteNumeric(480, "%s :Can't KNOCK on %s, +K is set.",user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		if (!c->IsModeSet(inviteonlymode))
		{
			user->WriteNumeric(480, "%s :Can't KNOCK on %s, channel is not invite only so knocking is pointless!",user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		if (sendnotice)
			c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :User %s is KNOCKing on %s (%s)", c->name.c_str(), user->nick.c_str(), c->name.c_str(), parameters[1].c_str());

		if (sendnumeric)
			c->WriteChannelWithServ(ServerInstance->Config->ServerName, "710 %s %s %s :is KNOCKing: %s", c->name.c_str(), c->name.c_str(), user->GetFullHost().c_str(), parameters[1].c_str());

		user->WriteNotice("KNOCKing on " + c->name);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleKnock : public Module
{
	SimpleChannelModeHandler kn;
	CommandKnock cmd;

 public:
	ModuleKnock()
		: kn(this, "noknock", 'K')
		, cmd(this, kn)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(kn);
		ServerInstance->Modules->AddService(cmd);

		ServerInstance->Modules->Attach(I_OnRehash, this);
		OnRehash(NULL);
	}

	void OnRehash(User* user) CXX11_OVERRIDE
	{
		std::string knocknotify = ServerInstance->Config->ConfValue("knock")->getString("notify");
		irc::string notify(knocknotify.c_str());

		if (notify == "numeric")
		{
			cmd.sendnotice = false;
			cmd.sendnumeric = true;
		}
		else if (notify == "both")
		{
			cmd.sendnotice = true;
			cmd.sendnumeric = true;
		}
		else
		{
			cmd.sendnotice = true;
			cmd.sendnumeric = false;
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for /KNOCK and channel mode +K", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleKnock)
