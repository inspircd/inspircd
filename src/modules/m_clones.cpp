/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "wildcard.h"

/* $ModDesc: Provides the /clones command to retrieve information on a user, channel, or IP address */

/** Handle /CHECK
 */
class cmd_clones : public command_t
{
 public:
 	cmd_clones (InspIRCd* Instance) : command_t(Instance,"CLONES", 'o', 1)
	{
		this->source = "m_clones.so";
		syntax = "<limit>";
	}

	std::string FindMatchingIP(const irc::string &ipaddr)
	{
		std::string n = assign(ipaddr);
		for (user_hash::const_iterator a = ServerInstance->clientlist->begin(); a != ServerInstance->clientlist->end(); a++)
			if (a->second->GetIPString() == n)
				return a->second->GetFullRealHost();
		return "<?>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{

		std::string clonesstr = "304 " + std::string(user->nick) + " :CLONES";

		unsigned long limit = atoi(parameters[0]);

		/*
		 * Syntax of a /clones reply:
		 *  :server.name 304 target :CLONES START
		 *  :server.name 304 target :CLONES <count> <ip> <fullhost>
		 *  :server.name 304 target :CHECK END
		 */

		user->WriteServ(clonesstr + " START");

		/* hostname or other */
		for (clonemap::iterator x = ServerInstance->global_clones.begin(); x != ServerInstance->global_clones.end(); x++)
		{
			if (x->second >= limit)
				user->WriteServ(clonesstr + " "+ ConvToStr(x->second) + " " + assign(x->first) + " " + FindMatchingIP(x->first));
		}

		user->WriteServ(clonesstr + " END");

		return CMD_LOCALONLY;
	}
};


class ModuleClones : public Module
{
 private:
	cmd_clones *mycommand;
 public:
	ModuleClones(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new cmd_clones(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleClones()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		/* we don't hook anything, nothing required */
	}
	
};

MODULE_INIT(ModuleClones)
