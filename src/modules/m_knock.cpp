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
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CMD_FAILURE;
		}

		if (c->HasUser(user))
		{
			user->WriteNumeric(ERR_KNOCKONCHAN, c->name, InspIRCd::Format("Can't KNOCK on %s, you are already on that channel.", c->name.c_str()));
			return CMD_FAILURE;
		}

		if (c->IsModeSet(noknockmode))
		{
			user->WriteNumeric(480, InspIRCd::Format("Can't KNOCK on %s, +K is set.", c->name.c_str()));
			return CMD_FAILURE;
		}

		if (!c->IsModeSet(inviteonlymode))
		{
			user->WriteNumeric(ERR_CHANOPEN, c->name, InspIRCd::Format("Can't KNOCK on %s, channel is not invite only so knocking is pointless!", c->name.c_str()));
			return CMD_FAILURE;
		}

		if (sendnotice)
			c->WriteNotice(InspIRCd::Format("User %s is KNOCKing on %s (%s)", user->nick.c_str(), c->name.c_str(), parameters[1].c_str()));

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

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		std::string knocknotify = ServerInstance->Config->ConfValue("knock")->getString("notify");
		if (stdalgo::string::equalsci(knocknotify, "numeric"))
		{
			cmd.sendnotice = false;
			cmd.sendnumeric = true;
		}
		else if (stdalgo::string::equalsci(knocknotify, "both"))
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
