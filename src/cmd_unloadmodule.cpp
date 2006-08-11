/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "commands.h"

#include "commands/cmd_unloadmodule.h"



void cmd_unloadmodule::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (ServerInstance->UnloadModule(parameters[0]))
	{
		ServerInstance->WriteOpers("*** MODULE UNLOADED: %s",parameters[0]);
		user->WriteServ("973 %s %s :Module successfully unloaded.",user->nick, parameters[0]);
	}
	else
	{
		user->WriteServ("972 %s %s :Failed to unload module: %s",user->nick, parameters[0],ServerInstance->ModuleError());
	}
}
