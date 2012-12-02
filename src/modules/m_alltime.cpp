/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 John Brooks <john.brooks@dereferenced.net>
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

/* $ModDesc: Display timestamps from all servers connected to the network */

class CommandAlltime : public Command
{
 public:
	CommandAlltime(Module* Creator) : Command(Creator, "ALLTIME", 0)
	{
		flags_needed = 'o';
		translation.push_back(TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		char fmtdate[64];
		time_t now = ServerInstance->Time();
		strftime(fmtdate, sizeof(fmtdate), "%Y-%m-%d %H:%M:%S", gmtime(&now));

		std::string msg = ":" + ServerInstance->Config->ServerName + " NOTICE " + user->nick + " :System time is " + fmtdate + " (" + ConvToStr(ServerInstance->Time()) + ") on " + ServerInstance->Config->ServerName;

		user->SendText(msg);

		/* we want this routed out! */
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_BCAST;
	}
};


class Modulealltime : public Module
{
	CommandAlltime mycommand;
 public:
	Modulealltime()
		: mycommand(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(mycommand);
	}

	virtual ~Modulealltime()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Display timestamps from all servers connected to the network", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(Modulealltime)
