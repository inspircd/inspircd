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

#include "users.h"
#include "commands/cmd_pass.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_pass(Instance);
}

void cmd_pass::Handle (const char** parameters, int pcnt, userrec *user)
{
	// Check to make sure they havnt registered -- Fix by FCS
	if (user->registered == REG_ALL)
	{
		user->WriteServ("462 %s :You may not reregister",user->nick);
		return;
	}
	ConnectClass a = user->GetClass();
	strlcpy(user->password,parameters[0],63);
	if (!strcmp(parameters[0],a.pass.c_str()))
	{
		user->haspassed = true;
	}
}
