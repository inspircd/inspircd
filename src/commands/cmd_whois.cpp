/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "hashcomp.h"

void do_whois(InspIRCd* ServerInstance, User* user, User* dest,unsigned long signon, unsigned long idle, const char* nick)
{
	if ((dest->Visibility && !dest->Visibility->VisibleTo(user)) || dest->registered != REG_ALL)
	{
		ServerInstance->SendWhoisLine(user, dest, 401, "%s %s :No such nick/channel",user->nick.c_str(), *nick ? nick : "*");
		ServerInstance->SendWhoisLine(user, dest, 318, "%s %s :End of /WHOIS list.",user->nick.c_str(), *nick ? nick : "*");
		return;
	}

	ServerInstance->SendWhoisLine(user, dest, 311, "%s %s %s %s * :%s",user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str(), dest->dhost.c_str(), dest->fullname.c_str());
	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		ServerInstance->SendWhoisLine(user, dest, 378, "%s %s :is connecting from %s@%s %s", user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str(), dest->host.c_str(), dest->GetIPString());
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
			ServerInstance->SendWhoisLine(user, dest, 319, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), cl.c_str());
		}
	}
	if (user != dest && *ServerInstance->Config->HideWhoisServer && !user->HasPrivPermission("servers/auspex"))
	{
		ServerInstance->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), ServerInstance->Config->HideWhoisServer, ServerInstance->Config->Network);
	}
	else
	{
		ServerInstance->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->server, ServerInstance->GetServerDescription(dest->server).c_str());
	}

	if (IS_AWAY(dest))
	{
		ServerInstance->SendWhoisLine(user, dest, 301, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->awaymsg.c_str());
	}

	if (IS_OPER(dest))
	{
		if (ServerInstance->Config->GenericOper)
			ServerInstance->SendWhoisLine(user, dest, 313, "%s %s :is an IRC operator",user->nick.c_str(), dest->nick.c_str());
		else
			ServerInstance->SendWhoisLine(user, dest, 313, "%s %s :is %s %s on %s",user->nick.c_str(), dest->nick.c_str(), (strchr("AEIOUaeiou",dest->oper[0]) ? "an" : "a"),irc::Spacify(dest->oper.c_str()), ServerInstance->Config->Network);
	}

	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		if (dest->IsModeSet('s') != 0)
		{
			ServerInstance->SendWhoisLine(user, dest, 379, "%s %s :is using modes +%s +%s", user->nick.c_str(), dest->nick.c_str(), dest->FormatModes(), dest->FormatNoticeMasks());
		}
		else
		{
			ServerInstance->SendWhoisLine(user, dest, 379, "%s %s :is using modes +%s", user->nick.c_str(), dest->nick.c_str(), dest->FormatModes());
		}
	}

	FOREACH_MOD(I_OnWhois,OnWhois(user,dest));

	/*
	 * We only send these if we've been provided them. That is, if hidewhois is turned off, and user is local, or
	 * if remote whois is queried, too. This is to keep the user hidden, and also since you can't reliably tell remote time. -- w00t
	 */
	if ((idle) || (signon))
	{
		ServerInstance->SendWhoisLine(user, dest, 317, "%s %s %lu %lu :seconds idle, signon time",user->nick.c_str(), dest->nick.c_str(), idle, signon);
	}

	ServerInstance->SendWhoisLine(user, dest, 318, "%s %s :End of /WHOIS list.",user->nick.c_str(), dest->nick.c_str());
}



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandWhois(Instance);
}

CmdResult CommandWhois::Handle (const std::vector<std::string>& parameters, User *user)
{
	User *dest;
	int userindex = 0;
	unsigned long idle = 0, signon = 0;

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;


	/*
	 * If 2 paramters are specified (/whois nick nick), ignore the first one like spanningtree
	 * does, and use the second one, otherwise, use the only paramter. -- djGrrr
	 */
	if (parameters.size() > 1)
		userindex = 1;

	if (IS_LOCAL(user))
		dest = ServerInstance->FindNickOnly(parameters[userindex]);
	else
		dest = ServerInstance->FindNick(parameters[userindex]);

	if (dest)
	{
		/*
		 * Okay. Umpteenth attempt at doing this, so let's re-comment...
		 * For local users (/w localuser), we show idletime if hidewhois is disabled
		 * For local users (/w localuser localuser), we always show idletime, hence parameters.size() > 1 check.
		 * For remote users (/w remoteuser), we do NOT show idletime
		 * For remote users (/w remoteuser remoteuser), spanningtree will handle calling do_whois, so we can ignore this case.
		 * Thanks to djGrrr for not being impatient while I have a crap day coding. :p -- w00t
		 */
		if (IS_LOCAL(dest) && (!*ServerInstance->Config->HideWhoisServer || parameters.size() > 1))
		{
			idle = abs((long)((dest->idle_lastmsg)-ServerInstance->Time()));
			signon = dest->signon;
		}

		do_whois(this->ServerInstance, user,dest,signon,idle,parameters[userindex].c_str());
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}


