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
#include "modules.h"

/* $ModDesc: Provides support for the SETIDENT command */

/** Handle /SETIDENT
 */
class cmd_setident : public command_t
{
 public:
 cmd_setident (InspIRCd* Instance) : command_t(Instance,"SETIDENT", 'o', 1)
	{
		this->source = "m_setident.so";
		syntax = "<new-ident>";
	}

	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		size_t len = 0;
		for(const char* x = parameters[0]; *x; x++, len++)
		{
			if(((*x >= 'A') && (*x <= '}')) || strchr(".-0123456789", *x))
				continue;

			user->WriteServ("NOTICE %s :*** Invalid characters in ident", user->nick);
			return CMD_FAILURE;
		}
		if (len == 0)
		{
			user->WriteServ("NOTICE %s :*** SETIDENT: Ident too short", user->nick);
			return CMD_FAILURE;
		}
		if (len > IDENTMAX)
		{
			user->WriteServ("NOTICE %s :*** Ident is too long", user->nick);
			return CMD_FAILURE;
		}

		user->ChangeIdent(parameters[0]);
		ServerInstance->WriteOpers("%s used SETIDENT to change their ident to '%s'", user->nick, user->ident);

		return CMD_SUCCESS;
	}
};


class ModuleSetIdent : public Module
{
	cmd_setident*	mycommand;
	
 public:
	ModuleSetIdent(InspIRCd* Me) : Module(Me)
	{
		
		mycommand = new cmd_setident(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR,API_VERSION);
	}
	
};


MODULE_INIT(ModuleSetIdent);
