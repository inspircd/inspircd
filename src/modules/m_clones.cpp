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
#include "wildcard.h"

/* $ModDesc: Provides the /clones command to retrieve information on a user, channel, or IP address */

/** Handle /CHECK
 */
class CommandClones : public Command
{
 public:
 	CommandClones (InspIRCd* Instance) : Command(Instance,"CLONES", 'o', 1)
	{
		this->source = "m_clones.so";
		syntax = "<limit>";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{

		std::string clonesstr = "304 " + std::string(user->nick) + " :CLONES";

		unsigned long limit = atoi(parameters[0]);

		/*
		 * Syntax of a /clones reply:
		 *  :server.name 304 target :CLONES START
		 *  :server.name 304 target :CLONES <count> <ip>
		 *  :server.name 304 target :CHECK END
		 */

		user->WriteServ(clonesstr + " START");

		/* hostname or other */
		// XXX I really don't like marking global_clones public for this. at all. -- w00t
		for (clonemap::iterator x = ServerInstance->Users->global_clones.begin(); x != ServerInstance->Users->global_clones.end(); x++)
		{
			if (x->second >= limit)
				user->WriteServ(clonesstr + " "+ ConvToStr(x->second) + " " + assign(x->first));
		}

		user->WriteServ(clonesstr + " END");

		return CMD_LOCALONLY;
	}
};


class ModuleClones : public Module
{
 private:
	CommandClones *mycommand;
 public:
	ModuleClones(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new CommandClones(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}
	
	virtual ~ModuleClones()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	
};

MODULE_INIT(ModuleClones)
