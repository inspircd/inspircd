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
#include "commands/cmd_trace.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_trace(Instance);
}

/** XXX: This is crap. someone fix this when you have time, to be more useful.
 */
CmdResult cmd_trace::Handle (const char** parameters, int pcnt, userrec *user)
{
	for (user_hash::iterator i = ServerInstance->clientlist->begin(); i != ServerInstance->clientlist->end(); i++)
	{
		if (i->second->registered == REG_ALL)
		{
			if (IS_OPER(i->second))
			{
				user->WriteServ("205 %s :Oper 0 %s",user->nick,i->second->nick);
			}
			else
			{
				user->WriteServ("204 %s :User 0 %s",user->nick,i->second->nick);
			}
		}
		else
		{
			user->WriteServ("203 %s :???? 0 [%s]",user->nick,i->second->host);
		}
	}
	return CMD_SUCCESS;
}
