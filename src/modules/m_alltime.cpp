/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	CommandAlltime(InspIRCd *Instance) : Command(Instance, "ALLTIME", "o", 0)
	{
		this->source = "m_alltime.so";
		syntax.clear();
		translation.push_back(TR_END);
	}

	CmdResult Handle(const char* const* parameters, int pcnt, User *user)
	{
		char fmtdate[64];
		time_t now = ServerInstance->Time();
		strftime(fmtdate, sizeof(fmtdate), "%F %T", gmtime(&now));
		
		std::string msg = ":" + std::string(ServerInstance->Config->ServerName) + " NOTICE " + user->nick + " :System time for " +
			ServerInstance->Config->ServerName + " is: " + fmtdate;
		
		if (IS_LOCAL(user))
		{
			user->Write(msg);
		}
		else
		{
			ServerInstance->PI->PushToClient(user, msg);
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
		return Version(1, 0, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(Modulealltime)
