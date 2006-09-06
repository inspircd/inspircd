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
#include "commands/cmd_userhost.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_userhost(Instance);
}

CmdResult cmd_userhost::Handle (const char** parameters, int pcnt, userrec *user)
{
	char Return[MAXBUF],junk[MAXBUF];
	snprintf(Return,MAXBUF,"302 %s :",user->nick);
	
	for (int i = 0; i < pcnt; i++)
	{
		userrec *u = ServerInstance->FindNick(parameters[i]);
		if ((u) && (u->registered == REG_ALL))
		{
			if(*u->oper)
				if(*user->oper)
					snprintf(junk,MAXBUF,"%s*=+%s@%s ",u->nick,u->ident,u->host);
				else
					snprintf(junk,MAXBUF,"%s*=+%s@%s ",u->nick,u->ident,u->dhost);
			else
				if(*user->oper)
					snprintf(junk,MAXBUF,"%s=+%s@%s ",u->nick,u->ident,u->host);
				else
					snprintf(junk,MAXBUF,"%s=+%s@%s ",u->nick,u->ident,u->dhost);

			strlcat(Return,junk,MAXBUF);
		}
	}
	user->WriteServ(Return);

	return CMD_SUCCESS;
}
