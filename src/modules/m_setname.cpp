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

/* $ModDesc: Provides support for the SETNAME command */



class cmd_setname : public command_t
{
 public:
	cmd_setname (InspIRCd* Instance) : command_t(Instance,"SETNAME", 0, 1)
	{
		this->source = "m_setname.so";
		syntax = "<new-gecos>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (!*parameters[0])
		{
			user->WriteServ("NOTICE %s :*** SETNAME: GECOS must be specified", user->nick);
			return CMD_FAILURE;
		}
		
		if (strlen(parameters[0]) > MAXGECOS)
		{
			user->WriteServ("NOTICE %s :*** SETNAME: GECOS too long", user->nick);
			return CMD_FAILURE;
		}
		
		if (user->ChangeName(parameters[0]))
		{
			ServerInstance->WriteOpers("%s used SETNAME to change their GECOS to %s", user->nick, parameters[0]);
			return CMD_SUCCESS;
		}

		return CMD_SUCCESS;
	}
};


class ModuleSetName : public Module
{
	cmd_setname*	mycommand;
 public:
	ModuleSetName(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_setname(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetName()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleSetName)
