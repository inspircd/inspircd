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
 *
 * Based on the UnrealIRCd 4.0 (1.1.x fork) module
 *
 * UnrealIRCd 4.0 (C) 2007 Carsten Valdemar Munk
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides /CLOSE functionality */

/** Handle /CLOSE
 */
class CommandClose : public Command
{
 public:
	/* Command 'close', needs operator */
	CommandClose(Module* Creator) : Command(Creator,"CLOSE", 0)
	{
	flags_needed = 'o'; }

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::map<std::string,int> closed;

		for (std::vector<User*>::iterator u = ServerInstance->Users->local_users.begin(); u != ServerInstance->Users->local_users.end(); u++)
		{
			if ((*u)->registered != REG_ALL)
			{
				ServerInstance->Users->QuitUser(*u, "Closing all unknown connections per request");
				std::string key = ConvToStr((*u)->GetIPString())+"."+ConvToStr((*u)->GetServerPort());
				closed[key]++;
			}
		}

		int total = 0;
		for (std::map<std::string,int>::iterator ci = closed.begin(); ci != closed.end(); ci++)
		{
			user->WriteServ("NOTICE %s :*** Closed %d unknown connection%s from [%s]",user->nick.c_str(),(*ci).second,((*ci).second>1)?"s":"",(*ci).first.c_str());
			total += (*ci).second;
		}
		if (total)
			user->WriteServ("NOTICE %s :*** %i unknown connection%s closed",user->nick.c_str(),total,(total>1)?"s":"");
		else
			user->WriteServ("NOTICE %s :*** No unknown connections found",user->nick.c_str());

		return CMD_SUCCESS;
	}
};

class ModuleClose : public Module
{
	CommandClose cmd;
 public:
	ModuleClose(InspIRCd* Me)
		: Module(Me), cmd(this)
	{
		ServerInstance->AddCommand(&cmd);
	}

	virtual ~ModuleClose()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleClose)
