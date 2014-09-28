/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Handle /WHOIS.
 */
class CommandWhois : public SplitCommand
{
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference snomaskmode;

	void SplitChanList(User* source, User* dest, const std::string& cl);
	void DoWhois(User* user, User* dest, unsigned long signon, unsigned long idle);
	std::string ChannelList(User* source, User* dest, bool spy);

 public:
	/** Constructor for whois.
	 */
	CommandWhois(Module* parent)
		: SplitCommand(parent, "WHOIS", 1)
		, secretmode(parent, "secret")
		, privatemode(parent, "private")
		, snomaskmode(parent, "snomask")
	{
		Penalty = 2;
		syntax = "<nick>{,<nick>}";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);
	CmdResult HandleRemote(const std::vector<std::string>& parameters, RemoteUser* target);
};

std::string CommandWhois::ChannelList(User* source, User* dest, bool spy)
{
	std::string list;

	for (User::ChanList::iterator i = dest->chans.begin(); i != dest->chans.end(); i++)
	{
		Membership* memb = *i;
		Channel* c = memb->chan;
		/* If the target is the sender, neither +p nor +s is set, or
		 * the channel contains the user, it is not a spy channel
		 */
		if (spy != (source == dest || !(c->IsModeSet(privatemode) || c->IsModeSet(secretmode)) || c->HasUser(source)))
		{
			char prefix = memb->GetPrefixChar();
			if (prefix)
				list.push_back(prefix);
			list.append(c->name).push_back(' ');
		}
	}

	return list;
}

void CommandWhois::SplitChanList(User* source, User* dest, const std::string& cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos;

	prefix << dest->nick << " :";
	line = prefix.str();
	int namelen = ServerInstance->Config->ServerName.length() + 6;

	for (start = 0; (pos = cl.find(' ', start)) != std::string::npos; start = pos+1)
	{
		if (line.length() + namelen + pos - start > 510)
		{
			ServerInstance->SendWhoisLine(source, dest, 319, line);
			line = prefix.str();
		}

		line.append(cl.substr(start, pos - start + 1));
	}

	if (line.length() != prefix.str().length())
	{
		ServerInstance->SendWhoisLine(source, dest, 319, line);
	}
}

void CommandWhois::DoWhois(User* user, User* dest, unsigned long signon, unsigned long idle)
{
	ServerInstance->SendWhoisLine(user, dest, 311, "%s %s %s * :%s", dest->nick.c_str(), dest->ident.c_str(), dest->dhost.c_str(), dest->fullname.c_str());
	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		ServerInstance->SendWhoisLine(user, dest, 378, "%s :is connecting from %s@%s %s", dest->nick.c_str(), dest->ident.c_str(), dest->host.c_str(), dest->GetIPString().c_str());
	}

	std::string cl = ChannelList(user, dest, false);
	const ServerConfig::OperSpyWhoisState state = user->HasPrivPermission("users/auspex") ? ServerInstance->Config->OperSpyWhois : ServerConfig::SPYWHOIS_NONE;

	if (state == ServerConfig::SPYWHOIS_SINGLEMSG)
		cl.append(ChannelList(user, dest, true));

	SplitChanList(user, dest, cl);

	if (state == ServerConfig::SPYWHOIS_SPLITMSG)
	{
		std::string scl = ChannelList(user, dest, true);
		if (scl.length())
		{
			ServerInstance->SendWhoisLine(user, dest, 336, "%s :is on private/secret channels:", dest->nick.c_str());
			SplitChanList(user, dest, scl);
		}
	}
	if (user != dest && !ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
	{
		ServerInstance->SendWhoisLine(user, dest, 312, "%s %s :%s", dest->nick.c_str(), ServerInstance->Config->HideWhoisServer.c_str(), ServerInstance->Config->Network.c_str());
	}
	else
	{
		ServerInstance->SendWhoisLine(user, dest, 312, "%s %s :%s", dest->nick.c_str(), dest->server->GetName().c_str(), dest->server->GetDesc().c_str());
	}

	if (dest->IsAway())
	{
		ServerInstance->SendWhoisLine(user, dest, 301, "%s :%s", dest->nick.c_str(), dest->awaymsg.c_str());
	}

	if (dest->IsOper())
	{
		if (ServerInstance->Config->GenericOper)
			ServerInstance->SendWhoisLine(user, dest, 313, "%s :is an IRC operator", dest->nick.c_str());
		else
			ServerInstance->SendWhoisLine(user, dest, 313, "%s :is %s %s on %s", dest->nick.c_str(), (strchr("AEIOUaeiou",dest->oper->name[0]) ? "an" : "a"),dest->oper->name.c_str(), ServerInstance->Config->Network.c_str());
	}

	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		if (dest->IsModeSet(snomaskmode))
		{
			ServerInstance->SendWhoisLine(user, dest, 379, "%s :is using modes +%s %s", dest->nick.c_str(), dest->FormatModes(), snomaskmode->GetUserParameter(dest).c_str());
		}
		else
		{
			ServerInstance->SendWhoisLine(user, dest, 379, "%s :is using modes +%s", dest->nick.c_str(), dest->FormatModes());
		}
	}

	FOREACH_MOD(OnWhois, (user,dest));

	/*
	 * We only send these if we've been provided them. That is, if hidewhois is turned off, and user is local, or
	 * if remote whois is queried, too. This is to keep the user hidden, and also since you can't reliably tell remote time. -- w00t
	 */
	if ((idle) || (signon))
	{
		ServerInstance->SendWhoisLine(user, dest, 317, "%s %lu %lu :seconds idle, signon time", dest->nick.c_str(), idle, signon);
	}

	ServerInstance->SendWhoisLine(user, dest, 318, "%s :End of /WHOIS list.", dest->nick.c_str());
}

CmdResult CommandWhois::HandleRemote(const std::vector<std::string>& parameters, RemoteUser* target)
{
	if (parameters.size() < 2)
		return CMD_FAILURE;

	User* user = ServerInstance->FindUUID(parameters[0]);
	if (!user)
		return CMD_FAILURE;

	unsigned long idle = ConvToInt(parameters.back());
	DoWhois(user, target, target->signon, idle);

	return CMD_SUCCESS;
}

CmdResult CommandWhois::HandleLocal(const std::vector<std::string>& parameters, LocalUser* user)
{
	User *dest;
	int userindex = 0;
	unsigned long idle = 0, signon = 0;

	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	/*
	 * If 2 paramters are specified (/whois nick nick), ignore the first one like spanningtree
	 * does, and use the second one, otherwise, use the only paramter. -- djGrrr
	 */
	if (parameters.size() > 1)
		userindex = 1;

	dest = ServerInstance->FindNickOnly(parameters[userindex]);

	if ((dest) && (dest->registered == REG_ALL))
	{
		/*
		 * Okay. Umpteenth attempt at doing this, so let's re-comment...
		 * For local users (/w localuser), we show idletime if hidewhois is disabled
		 * For local users (/w localuser localuser), we always show idletime, hence parameters.size() > 1 check.
		 * For remote users (/w remoteuser), we do NOT show idletime
		 * For remote users (/w remoteuser remoteuser), spanningtree will handle calling do_whois, so we can ignore this case.
		 * Thanks to djGrrr for not being impatient while I have a crap day coding. :p -- w00t
		 */
		LocalUser* localuser = IS_LOCAL(dest);
		if (localuser && (ServerInstance->Config->HideWhoisServer.empty() || parameters.size() > 1))
		{
			idle = abs((long)((localuser->idle_lastmsg)-ServerInstance->Time()));
			signon = dest->signon;
		}

		DoWhois(user,dest,signon,idle);
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(ERR_NOSUCHNICK, "%s :No such nick/channel", !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		user->WriteNumeric(RPL_ENDOFWHOIS, "%s :End of /WHOIS list.", !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWhois)
