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
#include "inspircd.h"
#include "commands/cmd_trace.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_trace(Instance);
}

CmdResult cmd_trace::Handle (const char** parameters, int pcnt, userrec *user)
{
	for (user_hash::iterator i = ServerInstance->clientlist.begin(); i != ServerInstance->clientlist.end(); i++)
	{
		if (i->second)
		{
			if (ServerInstance->IsNick(i->second->nick))
			{
				if (*i->second->oper)
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
	}
	return CMD_SUCCESS;
}
