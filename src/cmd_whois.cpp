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
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "hashcomp.h"

void do_whois(InspIRCd* ServerInstance, userrec* user, userrec* dest,unsigned long signon, unsigned long idle, const char* nick)
{
	if (dest->Visibility && !dest->Visibility->VisibleTo(user))
	{
		ServerInstance->SendWhoisLine(user, dest, 401, "%s %s :No such nick/channel",user->nick, *nick ? nick : "*");
		ServerInstance->SendWhoisLine(user, dest, 318, "%s %s :End of /WHOIS list.",user->nick, *nick ? nick : "*");
		return;
	}

	if (dest->registered == REG_ALL)
	{
		ServerInstance->SendWhoisLine(user, dest, 311, "%s %s %s %s * :%s",user->nick, dest->nick, dest->ident, dest->dhost, dest->fullname);
		if (user == dest || IS_OPER(user))
		{
			ServerInstance->SendWhoisLine(user, dest, 378, "%s %s :is connecting from %s@%s %s", user->nick, dest->nick, dest->ident, dest->host, dest->GetIPString());
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
				ServerInstance->SendWhoisLine(user, dest, 319, "%s %s :%s",user->nick, dest->nick, cl.c_str());
			}
		}
		if (*ServerInstance->Config->HideWhoisServer && !IS_OPER(user))
		{
			ServerInstance->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick, dest->nick, ServerInstance->Config->HideWhoisServer, ServerInstance->Config->Network);
		}
		else
		{
			ServerInstance->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick, dest->nick, dest->server, ServerInstance->GetServerDescription(dest->server).c_str());
		}

		if (IS_AWAY(dest))
		{
			ServerInstance->SendWhoisLine(user, dest, 301, "%s %s :%s",user->nick, dest->nick, dest->awaymsg);
		}

		if (IS_OPER(dest))
		{
			ServerInstance->SendWhoisLine(user, dest, 313, "%s %s :is %s %s on %s",user->nick, dest->nick, (strchr("AEIOUaeiou",*dest->oper) ? "an" : "a"),irc::Spacify(dest->oper), ServerInstance->Config->Network);
		}

		FOREACH_MOD(I_OnWhois,OnWhois(user,dest));

		/*
		 * We only send these if we've been provided them. That is, if hidewhois is turned off, and user is local, or
		 * if remote whois is queried, too. This is to keep the user hidden, and also since you can't reliably tell remote time. -- w00t
		 */
		if ((idle) || (signon))
		{
			ServerInstance->SendWhoisLine(user, dest, 317, "%s %s %d %d :seconds idle, signon time",user->nick, dest->nick, idle, signon);
		}

		ServerInstance->SendWhoisLine(user, dest, 318, "%s %s :End of /WHOIS list.",user->nick, dest->nick);
	}
	else
	{
		ServerInstance->SendWhoisLine(user, dest, 401, "%s %s :No such nick/channel",user->nick, *nick ? nick : "*");
		ServerInstance->SendWhoisLine(user, dest, 318, "%s %s :End of /WHOIS list.",user->nick, *nick ? nick : "*");
	}
}



extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_whois(Instance);
}

CmdResult cmd_whois::Handle (const char** parameters, int pcnt, userrec *user)
{
	userrec *dest;
	int userindex = 0;
	unsigned long idle = 0, signon = 0;

	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;


	/*
	 * If 2 paramters are specified (/whois nick nick), ignore the first one like spanningtree
	 * does, and use the second one, otherwise, use the only paramter. -- djGrrr
	 */
	if (pcnt > 1)
		userindex = 1;

	dest = ServerInstance->FindNick(parameters[userindex]);

	if (dest)
	{
		/*
		 * Okay. Umpteenth attempt at doing this, so let's re-comment...
		 * For local users (/w localuser), we show idletime if hidewhois is disabled
		 * For local users (/w localuser localuser), we always show idletime, hence pcnt > 1 check.
		 * For remote users (/w remoteuser), we do NOT show idletime
		 * For remote users (/w remoteuser remoteuser), spanningtree will handle calling do_whois, so we can ignore this case.
		 * Thanks to djGrrr for not being impatient while I have a crap day coding. :p -- w00t
		 */
		if (IS_LOCAL(dest) && (!*ServerInstance->Config->HideWhoisServer || pcnt > 1))
		{
			idle = abs((dest->idle_lastmsg)-ServerInstance->Time());
			signon = dest->signon;
		}

		do_whois(this->ServerInstance, user,dest,signon,idle,parameters[userindex]);
	}
	else
	{
		/* no such nick/channel */
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, *parameters[userindex] ? parameters[userindex] : "*");
		user->WriteServ("318 %s %s :End of /WHOIS list.",user->nick, *parameters[userindex] ? parameters[userindex] : "*");
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

