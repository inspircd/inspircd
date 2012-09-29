/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	const ServerConfig::OperSpyWhoisState state = user->HasPrivPermission("users/auspex") ? ServerInstance->Config->OperSpyWhois : ServerConfig::SPYWHOIS_NONE;

	if (state == ServerConfig::SPYWHOIS_SINGLEMSG)
		cl.append(dest->ChannelList(user, true));

	user->SplitChanList(dest,cl);

	if (state == ServerConfig::SPYWHOIS_SPLITMSG)
	{
		std::string scl = dest->ChannelList(user, true);
		if (scl.length())
		{
			SendWhoisLine(user, dest, 336, "%s %s :is on private/secret channels:",user->nick.c_str(), dest->nick.c_str());
			user->SplitChanList(dest,scl);
		}
	}
	if (user != dest && !this->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
	{
		this->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), this->Config->HideWhoisServer.c_str(), this->Config->Network.c_str());
	}
	else
	{
		std::string serverdesc = GetServerDescription(dest->server);
		this->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->server.c_str(), serverdesc.c_str());
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



