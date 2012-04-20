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
	CommandAlltime(InspIRCd *Instance) : Command(Instance, "ALLTIME", "o", 0)
	{
		this->source = "m_alltime.so";
		syntax.clear();
		translation.push_back(TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		char fmtdate[64];
		time_t now = ServerInstance->Time();
		strftime(fmtdate, sizeof(fmtdate), "%Y-%m-%d %H:%M:%S", gmtime(&now));

		std::string msg = ":" + std::string(ServerInstance->Config->ServerName) + " NOTICE " + user->nick + " :System time is " + fmtdate + "(" + ConvToStr(ServerInstance->Time()) + ") on " + ServerInstance->Config->ServerName;

		if (IS_LOCAL(user))
		{
			user->Write(msg);
		}
		else
		{
			ServerInstance->PI->PushToClient(user, ":" + msg);
		}

		/* we want this routed out! */
		return CMD_SUCCESS;
	}
};


class Modulealltime : public Module
{
	CommandAlltime *mycommand;
 public:
	Modulealltime(InspIRCd *Me)
		: Module(Me)
	{
		mycommand = new CommandAlltime(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~Modulealltime()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(Modulealltime)
