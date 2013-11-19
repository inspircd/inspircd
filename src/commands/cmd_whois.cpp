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

/** Handle /WHOIS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
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
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* user);
	CmdResult HandleRemote(const std::vector<std::string>& parameters, RemoteUser* target);
};

std::string CommandWhois::ChannelList(User* source, User* dest, bool spy)
{
	std::string list;

	for (UCListIter i = dest->chans.begin(); i != dest->chans.end(); i++)
	{
		Channel* c = *i;
		/* If the target is the sender, neither +p nor +s is set, or
		 * the channel contains the user, it is not a spy channel
		 */
		if (spy != (source == dest || !(c->IsModeSet(privatemode) || c->IsModeSet(secretmode)) || c->HasUser(source)))
			list.append(c->GetPrefixChar(dest)).append(c->name).append(" ");
	}

	return list;
}

void CommandWhois::SplitChanList(User* source, User* dest, const std::string& cl)
{
	std::string line;
	std::ostringstream prefix;
	std::string::size_type start, pos, length;

	prefix << source->nick << " " << dest->nick << " :";
	line = prefix.str();
	int namelen = ServerInstance->Config->ServerName.length() + 6;

	for (start = 0; (pos = cl.find(' ', start)) != std::string::npos; start = pos+1)
	{
		length = (pos == std::string::npos) ? cl.length() : pos;

		if (line.length() + namelen + length - start > 510)
		{
			ServerInstance->SendWhoisLine(source, dest, 319, "%s", line.c_str());
			line = prefix.str();
		}

		if(pos == std::string::npos)
		{
			line.append(cl.substr(start, length - start));
			break;
		}
		else
		{
			line.append(cl.substr(start, length - start + 1));
		}
	}

	if (line.length() != prefix.str().length())
	{
		ServerInstance->SendWhoisLine(source, dest, 319, "%s", line.c_str());
	}
}

void CommandWhois::DoWhois(User* user, User* dest, unsigned long signon, unsigned long idle)
{
	ServerInstance->SendWhoisLine(user, dest, 311, "%s %s %s %s * :%s",user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str(), dest->dhost.c_str(), dest->fullname.c_str());
	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		ServerInstance->SendWhoisLine(user, dest, 378, "%s %s :is connecting from %s@%s %s", user->nick.c_str(), dest->nick.c_str(), dest->ident.c_str(), dest->host.c_str(), dest->GetIPString().c_str());
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
			ServerInstance->SendWhoisLine(user, dest, 336, "%s %s :is on private/secret channels:",user->nick.c_str(), dest->nick.c_str());
			SplitChanList(user, dest, scl);
		}
	}
	if (user != dest && !ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
	{
		ServerInstance->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), ServerInstance->Config->HideWhoisServer.c_str(), ServerInstance->Config->Network.c_str());
	}
	else
	{
		std::string serverdesc = ServerInstance->GetServerDescription(dest->server);
		ServerInstance->SendWhoisLine(user, dest, 312, "%s %s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->server.c_str(), serverdesc.c_str());
	}

	if (dest->IsAway())
	{
		ServerInstance->SendWhoisLine(user, dest, 301, "%s %s :%s",user->nick.c_str(), dest->nick.c_str(), dest->awaymsg.c_str());
	}

	if (dest->IsOper())
	{
		if (ServerInstance->Config->GenericOper)
			ServerInstance->SendWhoisLine(user, dest, 313, "%s %s :is an IRC operator",user->nick.c_str(), dest->nick.c_str());
		else
			ServerInstance->SendWhoisLine(user, dest, 313, "%s %s :is %s %s on %s",user->nick.c_str(), dest->nick.c_str(), (strchr("AEIOUaeiou",dest->oper->name[0]) ? "an" : "a"),dest->oper->name.c_str(), ServerInstance->Config->Network.c_str());
	}

	if (user == dest || user->HasPrivPermission("users/auspex"))
	{
		if (dest->IsModeSet(snomaskmode))
		{
			ServerInstance->SendWhoisLine(user, dest, 379, "%s %s :is using modes +%s %s", user->nick.c_str(), dest->nick.c_str(), dest->FormatModes(), snomaskmode->GetUserParameter(dest).c_str());
		}
		else
		{
			ServerInstance->SendWhoisLine(user, dest, 379, "%s %s :is using modes +%s", user->nick.c_str(), dest->nick.c_str(), dest->FormatModes());
		}
	}

	FOREACH_MOD(OnWhois, (user,dest));

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
	time_t idle = 0, signon = 0;

	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	dest = ServerInstance->FindNickOnly(parameters[parameters.size() == 1 ? 0 : 1]);

	if (dest && dest->registered == REG_ALL)
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
			idle = ServerInstance->Time() - localuser->idle_lastmsg;
			signon = dest->signon;
		}

		DoWhois(user,dest,signon,idle);
	}
	else
	{
		/* no such nick/channel */
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), !parameters[userindex].empty() ? parameters[userindex].c_str() : "*");
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWhois)
