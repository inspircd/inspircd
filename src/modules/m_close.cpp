/*       +------------------------------------+
 *       | UnrealIRCd v4.0                    |
 *       +------------------------------------+
 *
 * UnrealIRCd 4.0 (C) 2007 Carsten Valdemar Munk 
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides /CLOSE functionality */

/** Handle /CLOSE
 */
class cmd_close : public command_t
{
 public:
	/* Command 'close', needs operator */
	cmd_close (InspIRCd* Instance) : command_t(Instance,"CLOSE", 'o', 0)
	{
		this->source = "m_close.so";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		std::map<std::string,int> closed;

		for (std::vector<userrec*>::iterator u = ServerInstance->local_users.begin(); u != ServerInstance->local_users.end(); u++)
		{
			if ((*u)->registered != REG_ALL)
			{
				userrec::QuitUser(ServerInstance, *u, "Closing all unknown connections per request");
				std::string key = ConvToStr((*u)->GetIPString())+"."+ConvToStr((*u)->GetPort());
				closed[key]++;
			}
		}

		int total = 0;
		for (std::map<std::string,int>::iterator ci = closed.begin(); ci != closed.end(); ci++)
		{
			user->WriteServ("NOTICE %s :*** Closed %d unknown connection%s from [%s]",user->nick,(*ci).second,((*ci).second>1)?"s":"",(*ci).first.c_str());
			total += (*ci).second;
		}
		if (total)
			user->WriteServ("NOTICE %s :*** %i unknown connection%s closed",user->nick,total,(total>1)?"s":"");
		else
			user->WriteServ("NOTICE %s :*** No unknown connections found",user->nick);
			
		return CMD_LOCALONLY;
	}
};

class ModuleClose : public Module
{
	cmd_close* newcommand;
 public:
	ModuleClose(InspIRCd* Me)
		: Module(Me)
	{
		// Create a new command
		newcommand = new cmd_close(ServerInstance);
		ServerInstance->AddCommand(newcommand);
	}

	virtual ~ModuleClose()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleClose)
