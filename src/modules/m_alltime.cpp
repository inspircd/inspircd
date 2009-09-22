/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
		flags_needed = 'o'; syntax.clear();
		translation.push_back(TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		char fmtdate[64];
		time_t now = ServerInstance->Time();
		strftime(fmtdate, sizeof(fmtdate), "%Y-%m-%d %H:%M:%S", gmtime(&now));

		std::string msg = ":" + std::string(ServerInstance->Config->ServerName) + " NOTICE " + user->nick + " :System time is " + fmtdate + "(" + ConvToStr(ServerInstance->Time()) + ") on " + ServerInstance->Config->ServerName;

		ServerInstance->DumpText(user, msg);

		/* we want this routed out! */
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};


class Modulealltime : public Module
{
	CommandAlltime mycommand;
 public:
	Modulealltime(InspIRCd *Me)
		: Module(Me), mycommand(this)
	{
		ServerInstance->AddCommand(&mycommand);
	}

	virtual ~Modulealltime()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Display timestamps from all servers connected to the network", VF_COMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(Modulealltime)
