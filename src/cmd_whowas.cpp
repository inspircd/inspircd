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

#include "configreader.h"
#include "users.h"
#include "commands/cmd_whowas.h"

void cmd_whowas::Handle (const char** parameters, int pcnt, userrec* user)
{
	irc::whowas::whowas_users::iterator i = ServerInstance->whowas.find(parameters[0]);

	if (i == ServerInstance->whowas.end())
	{
		user->WriteServ("406 %s %s :There was no such nickname",user->nick,parameters[0]);
	}
	else
	{
		irc::whowas::whowas_set* grp = i->second;
		if (grp->size())
		{
			for (irc::whowas::whowas_set::iterator ux = grp->begin(); ux != grp->end(); ux++)
			{
				irc::whowas::WhoWasGroup* u = *ux;
				time_t rawtime = u->signon;
				tm *timeinfo;
				char b[MAXBUF];
	
				timeinfo = localtime(&rawtime);
				
				/* XXX - 'b' could be only 25 chars long and then strlcpy() would terminate it for us too? */
				strlcpy(b,asctime(timeinfo),MAXBUF);
				b[24] = 0;

				user->WriteServ("314 %s %s %s %s * :%s",user->nick,parameters[0],u->ident,u->dhost,u->gecos);
				
				if(*user->oper)
					user->WriteServ("379 %s %s :was connecting from *@%s", user->nick, parameters[0], u->host);
				
				if(*ServerInstance->Config->HideWhoisServer && !(*user->oper))
					user->WriteServ("312 %s %s %s :%s",user->nick,parameters[0], ServerInstance->Config->HideWhoisServer, b);
				else
					user->WriteServ("312 %s %s %s :%s",user->nick,parameters[0], u->server, b);
			}
		}
		else
		{
			user->WriteServ("406 %s %s :There was no such nickname",user->nick,parameters[0]);
		}
	}
	
	user->WriteServ("369 %s %s :End of WHOWAS",user->nick,parameters[0]);
}
