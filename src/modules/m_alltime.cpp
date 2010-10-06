/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

		std::string msg = ":" + std::string(ServerInstance->Config->ServerName.c_str()) + " NOTICE " + user->nick + " :System time is " + fmtdate + "(" + ConvToStr(ServerInstance->Time()) + ") on " + ServerInstance->Config->ServerName;

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
	Modulealltime() : mycommand(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(mycommand);
	}

	Version GetVersion()
	{
		return Version("Display timestamps from all servers connected to the network", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(Modulealltime)
