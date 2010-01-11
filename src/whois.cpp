/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

void InspIRCd::DoWhois(User* user, User* dest,unsigned long signon, unsigned long idle, const char* nick)
{
	this->SendWhoisLine(user, dest, 311, "%s %s %s %s * :%s",user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str(), dest->dhost.c_str(), dest->fullname.c_str());
	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		this->SendWhoisLine(user, dest, 378, "%s %s :is connecting from %s@%s %s", user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str(), dest->host.c_str(), dest->GetIPString());
	}

	std::string cl = dest->ChannelList(user, false);

	if (cl.length())
		user->SplitChanList(dest,cl);
	if (IS_OPER(user) && ServerInstance->Config->OperSpyWhois)
	{
		std::string scl = dest->ChannelList(user, true);
		if (scl.length())
		{
			this->SendWhoisLine(user, dest, 336, "%s %s :is on private/secret channels:",user->nick.c_str(), dest->nick.c_str());
			user->SplitChanList(dest,scl);
		}
	}
	if (user != dest && !this->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
	{
		this->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), this->Config->HideWhoisServer.c_str(), this->Config->Network.c_str());
	}
	else
	{
		this->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->server.c_str(), this->GetServerDescription(dest->server).c_str());
	}

	if (IS_AWAY(dest))
	{
		this->SendWhoisLine(user, dest, 301, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->awaymsg.c_str());
	}

	if (IS_OPER(dest))
	{
		if (this->Config->GenericOper)
			this->SendWhoisLine(user, dest, 313, "%s %s :is an IRC operator",user->nick.c_str(), dest->nick.c_str());
		else
			this->SendWhoisLine(user, dest, 313, "%s %s :is %s %s on %s",user->nick.c_str(), dest->nick.c_str(), (strchr("AEIOUaeiou",dest->oper->name[0]) ? "an" : "a"),dest->oper->NameStr(), this->Config->Network.c_str());
	}

	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		if (dest->IsModeSet('s') != 0)
		{
			this->SendWhoisLine(user, dest, 379, "%s %s :is using modes +%s +%s", user->nick.c_str(), dest->nick.c_str(), dest->FormatModes(), dest->FormatNoticeMasks());
		}
		else
		{
			this->SendWhoisLine(user, dest, 379, "%s %s :is using modes +%s", user->nick.c_str(), dest->nick.c_str(), dest->FormatModes());
		}
	}

	FOREACH_MOD(I_OnWhois,OnWhois(user,dest));

	/*
	 * We only send these if we've been provided them. That is, if hidewhois is turned off, and user is local, or
	 * if remote whois is queried, too. This is to keep the user hidden, and also since you can't reliably tell remote time. -- w00t
	 */
	if ((idle) || (signon))
	{
		this->SendWhoisLine(user, dest, 317, "%s %s %lu %lu :seconds idle, signon time",user->nick.c_str(), dest->nick.c_str(), idle, signon);
	}

	this->SendWhoisLine(user, dest, 318, "%s %s :End of /WHOIS list.",user->nick.c_str(), dest->nick.c_str());
}



