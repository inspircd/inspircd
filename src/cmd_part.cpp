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
#include "commands/cmd_part.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_part(Instance);
}

CmdResult cmd_part::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	chanrec* c = ServerInstance->FindChan(parameters[0]);
	
	if (c)
	{
		if (!c->PartUser(user, pcnt > 1 ? parameters[1] : NULL))
			/* Arse, who stole our channel! :/ */
			delete c;
	}
	else
	{
		user->WriteServ( "401 %s %s :No such channel", user->nick, parameters[0]);
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
