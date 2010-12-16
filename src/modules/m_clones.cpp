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

/* $ModDesc: Provides the /clones command to retrieve information on clones. */

/** Handle /CHECK
 */
class CommandClones : public Command
{
 public:
 	CommandClones(Module* Creator) : Command(Creator,"CLONES", 1)
	{
		flags_needed = 'o'; syntax = "<limit>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{

		std::string clonesstr = "304 " + std::string(user->nick) + " :CLONES";

		unsigned long limit = atoi(parameters[0].c_str());

		/*
		 * Syntax of a /clones reply:
		 *  :server.name 304 target :CLONES START
		 *  :server.name 304 target :CLONES <count> <ip>
		 *  :server.name 304 target :CHECK END
		 */

		user->WriteServ("%s START", clonesstr.c_str());

		/* hostname or other */
		// XXX I really don't like marking global_clones public for this. at all. -- w00t
		for (clonemap::iterator x = ServerInstance->Users->global_clones.begin(); x != ServerInstance->Users->global_clones.end(); x++)
		{
			if (x->second >= limit)
				user->WriteServ("%s %s %s", clonesstr.c_str(), ConvToStr(x->second).c_str(), x->first.str().c_str());
		}

		user->WriteServ("%s END", clonesstr.c_str());

		return CMD_SUCCESS;
	}
};


class ModuleClones : public Module
{
 private:
	CommandClones cmd;
 public:
	ModuleClones() : cmd(this)
	{}

	void init()
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleClones()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides the /clones command to retrieve information on clones.", VF_VENDOR);
	}


};

MODULE_INIT(ModuleClones)
