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

#include "inspircd_config.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_userhost.h"

void cmd_userhost::Handle (const char** parameters, int pcnt, userrec *user)
{
	char Return[MAXBUF],junk[MAXBUF];
	snprintf(Return,MAXBUF,"302 %s :",user->nick);
	
	for (int i = 0; i < pcnt; i++)
	{
		userrec *u = Find(parameters[i]);
		if(u)
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
}
