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
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"

#include "commands/cmd_whois.h"

const char* Spacify(char* n)
{
	static char x[MAXBUF];
	strlcpy(x,n,MAXBUF);
	for (char* y = x; *y; y++)
		if (*y == '_')
			*y = ' ';
	return x;
}

void do_whois(InspIRCd* ServerInstance, userrec* user, userrec* dest,unsigned long signon, unsigned long idle, const char* nick)
{
	// bug found by phidjit - were able to whois an incomplete connection if it had sent a NICK or USER
	if (dest->registered == REG_ALL)
	{
		user->WriteServ("311 %s %s %s %s * :%s",user->nick, dest->nick, dest->ident, dest->dhost, dest->fullname);
		if ((user == dest) || (*user->oper))
		{
			user->WriteServ("378 %s %s :is connecting from *@%s %s",user->nick, dest->nick, dest->host, dest->GetIPString());
		}
		std::string cl = dest->ChannelList(user);
		if (cl.length())
		{
			if (cl.length() > 400)
			{
				user->SplitChanList(dest,cl);
			}
			else
			{
				user->WriteServ("319 %s %s :%s",user->nick, dest->nick, cl.c_str());
			}
		}
		if (*ServerInstance->Config->HideWhoisServer && !(*user->oper))
		{
			user->WriteServ("312 %s %s %s :%s",user->nick, dest->nick, ServerInstance->Config->HideWhoisServer, ServerInstance->Config->Network);
		}
		else
		{
			user->WriteServ("312 %s %s %s :%s",user->nick, dest->nick, dest->server, ServerInstance->GetServerDescription(dest->server).c_str());
		}
		if (*dest->awaymsg)
		{
			user->WriteServ("301 %s %s :%s",user->nick, dest->nick, dest->awaymsg);
		}
		if (*dest->oper)
		{
			user->WriteServ("313 %s %s :is %s %s on %s",user->nick, dest->nick, (strchr("AEIOUaeiou",*dest->oper) ? "an" : "a"),Spacify(dest->oper), ServerInstance->Config->Network);
		}
		if ((!signon) && (!idle))
		{
			FOREACH_MOD(I_OnWhois,OnWhois(user,dest));
		}
		if (!strcasecmp(user->server,dest->server))
		{
			// idle time and signon line can only be sent if youre on the same server (according to RFC)
			user->WriteServ("317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, abs((dest->idle_lastmsg)-ServerInstance->Time()), dest->signon);
		}
		else
		{
			if ((idle) || (signon))
				user->WriteServ("317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, idle, signon);
		}
		user->WriteServ("318 %s %s :End of /WHOIS list.",user->nick, dest->nick);
	}
	else
	{
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, nick);
		user->WriteServ("318 %s %s :End of /WHOIS list.",user->nick, nick);
	}
}

void cmd_whois::Handle (const char** parameters, int pcnt, userrec *user)
{
	userrec *dest;
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return;

	dest = ServerInstance->FindNick(parameters[0]);
	if (dest)
	{
		do_whois(this->ServerInstance, user,dest,0,0,parameters[0]);
	}
	else
	{
		/* no such nick/channel */
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
		user->WriteServ("318 %s %s :End of /WHOIS list.",user->nick, parameters[0]);
	}
}

