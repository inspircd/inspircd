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
#include "configreader.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_whowas.h"

extern ServerConfig* Config;
extern whowas_users whowas;

void cmd_whowas::Handle (char **parameters, int pcnt, userrec* user)
{
	whowas_users::iterator i = whowas.find(parameters[0]);

	if (i == whowas.end())
	{
		WriteServ(user->fd,"406 %s %s :There was no such nickname",user->nick,parameters[0]);
	}
	else
	{
		whowas_set* grp = i->second;
		if (grp->size())
		{
			for (whowas_set::iterator ux = grp->begin(); ux != grp->end(); ux++)
			{
				WhoWasGroup* u = *ux;
				time_t rawtime = u->signon;
				tm *timeinfo;
				char b[MAXBUF];
	
				timeinfo = localtime(&rawtime);
				
				/* XXX - 'b' could be only 25 chars long and then strlcpy() would terminate it for us too? */
				strlcpy(b,asctime(timeinfo),MAXBUF);
				b[24] = 0;

				WriteServ(user->fd,"314 %s %s %s %s * :%s",user->nick,parameters[0],u->ident,u->dhost,u->gecos);
				
				if(*user->oper)
					WriteServ(user->fd,"379 %s %s :was connecting from *@%s", user->nick, parameters[0], u->host);
				
				if(*Config->HideWhoisServer && !(*user->oper))
					WriteServ(user->fd,"312 %s %s %s :%s",user->nick,parameters[0], Config->HideWhoisServer, b);
				else
					WriteServ(user->fd,"312 %s %s %s :%s",user->nick,parameters[0], u->server, b);
			}
		}
		else
		{
			WriteServ(user->fd,"406 %s %s :There was no such nickname",user->nick,parameters[0]);
		}
	}
	
	WriteServ(user->fd,"369 %s %s :End of WHOWAS",user->nick,parameters[0]);
}
