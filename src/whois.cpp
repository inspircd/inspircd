/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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

	std::string cl = dest->ChannelList(user);

	if (cl.length())
	{
		if (cl.length() > 400)
		{
			user->SplitChanList(dest,cl);
		}
		else
		{
			this->SendWhoisLine(user, dest, 319, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), cl.c_str());
		}
	}
	if (user != dest && *this->Config->HideWhoisServer && !user->HasPrivPermission("servers/auspex"))
	{
		this->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), this->Config->HideWhoisServer, this->Config->Network);
	}
	else
	{
		this->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->server, this->GetServerDescription(dest->server).c_str());
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
			this->SendWhoisLine(user, dest, 313, "%s %s :is %s %s on %s",user->nick.c_str(), dest->nick.c_str(), (strchr("AEIOUaeiou",dest->oper[0]) ? "an" : "a"),irc::Spacify(dest->oper.c_str()), this->Config->Network);
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

	FOREACH_MOD_I(this, I_OnWhois,OnWhois(user,dest));

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



