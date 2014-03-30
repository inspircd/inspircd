/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/account.h"

struct WhoData
{
	std::string initial;
	std::vector<std::string> results;

	std::string matchtext;
	bool wildcards;

	std::bitset<256> flags;

	bool whox;
	std::string querytext;
	std::bitset<256> whox_flags;

	WhoData()
	{
		wildcards = false;
		whox = false;
	}
};

/** Handle /WHO. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWho : public Command
{
	bool CanView(Channel* chan, User* user);

	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference invisiblemode;

	Membership* GetFirstVisibleChannel(User *u)
	{
		for (UCListIter i = u->chans.begin(); i != u->chans.end(); ++i)
		{
			Membership* memb = *i;
			if (!memb->chan->IsModeSet(secretmode))
				return memb;
		}
		return NULL;
	}

	/** Does the target match the given Whodata?
	 */
	bool Match(User* source, User* target, WhoData &data);

	/** /who on a channel
	 */
	void WhoChannel(User* source, const std::vector<std::string>& parameters, Channel* c, WhoData &data);

	template<typename T>
	static User* GetUser(T& t);

	template<typename T>
	void WhoUsers(User *source, const std::vector<std::string>& parameters, T& users, WhoData &data);

 public:
	/** Constructor for who.
	 */
	CommandWho(Module* parent)
		: Command(parent, "WHO", 1)
		, secretmode(parent, "secret")
		, privatemode(parent, "private")
		, invisiblemode(parent, "invisible")
	{
		syntax = "<server>|<nickname>|<channel>|<realname>|<host>|0 [ohurmMiaplf]";
	}

	void SendWhoLine(User* source, const std::vector<std::string>& parms, Membership* memb, User* u, WhoData &data);

	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

template<> User* CommandWho::GetUser(std::list<User *>::iterator& t) { return *t; }
template<> User* CommandWho::GetUser(TR1NS::unordered_map<std::string, User*, irc::insensitive, irc::StrHashComp>::iterator& t) { return t->second; }

bool CommandWho::Match(User* source, User* user, WhoData &data)
{
	bool match = false;

	if (user->registered != REG_ALL)
		return false;

	if (data.flags['l'] && (source->HasPrivPermission("users/auspex") || ServerInstance->Config->HideWhoisServer.empty()) && !IS_LOCAL(user))
		return false;
	if (data.flags['f'] && (source->HasPrivPermission("users/auspex") || ServerInstance->Config->HideWhoisServer.empty()) && IS_LOCAL(user))
		return false;

	if (data.flags['m'] && source-> HasPrivPermission("users/auspex"))
	{
		bool positive = true;
		for (unsigned i = 0; i < data.matchtext.length(); ++i)
		{
			char c = data.matchtext[i];
			if (c == '+')
				positive = true;
			else if (c == '-')
				positive = false;
			else if (user->IsModeSet(c) != positive)
				return false;
		}
		return true;
	}

	/*
	 * This was previously one awesome pile of ugly nested if, when really, it didn't need
	 * to be, since only one condition was ever checked, a chained if works just fine.
	 * -- w00t
	 */
	if (data.flags['M'] && source->HasPrivPermission("users/auspex"))
	{
		const Extensible::ExtensibleStore& list = user->GetExtList();
		for(Extensible::ExtensibleStore::const_iterator i = list.begin(); i != list.end(); ++i)
			if (InspIRCd::Match(i->first->name, data.matchtext))
				match = true;
	}
	else if (data.flags['r'])
		match = InspIRCd::Match(user->fullname, data.matchtext);
	else if (data.flags['h'] && source->HasPrivPermission("users/auspex"))
		match = InspIRCd::Match(user->host, data.matchtext, ascii_case_insensitive_map);
	else if (data.flags['i'])
		match = InspIRCd::Match(user->ident, data.matchtext, ascii_case_insensitive_map);
	else if (data.flags['p'] && source->HasPrivPermission("users/auspex"))
	{
		irc::portparser portrange(data.matchtext, false);
		long portno = -1;
		while ((portno = portrange.GetToken()))
			if (IS_LOCAL(user) && portno == IS_LOCAL(user)->GetServerPort())
			{
				match = true;
				break;
			}
	}
	else if (data.flags['a'])
		match = InspIRCd::Match(user->awaymsg, data.matchtext);
	else if (data.flags['t'])
	{
		long seconds = InspIRCd::Duration(data.matchtext);

		// Okay, so time matching, we want all users connected `seconds' ago
		if (user->age >= ServerInstance->Time() - seconds)
			match = true;
	}

	/*
	 * Once the conditionals have been checked, only check dhost/nick/server
	 * if they didn't match this user -- and only match if we don't find a match.
	 *
	 * This should make things minutely faster, and again, less ugly.
	 * -- w00t
	 */
	if (!match)
		match = InspIRCd::Match(user->dhost, data.matchtext, ascii_case_insensitive_map);

	if (!match)
		match = InspIRCd::Match(user->nick, data.matchtext);

	/* Don't allow server name matches if HideWhoisServer is enabled, unless the command user has the priv */
	if (!match && (ServerInstance->Config->HideWhoisServer.empty() || source->HasPrivPermission("users/auspex")))
		match = InspIRCd::Match(user->server->GetName(), data.matchtext);

	return match;
}

void CommandWho::WhoChannel(User* source, const std::vector<std::string>& parameters, Channel* ch, WhoData &data)
{
	if (!CanView(ch, source))
		return;

	bool inside = ch->HasUser(source);

	/* who on a channel. */
	const UserMembList *cu = ch->GetUsers();

	for (UserMembCIter i = cu->begin(); i != cu->end(); i++)
	{
		User *u = i->first;

		/* None of this applies if we WHO ourselves */
		if (source != u)
		{
			/* opers only, please */
			if (data.flags['o'] && !u->IsOper())
				continue;

			/* If we're not inside the channel, hide +i users */
			if (u->IsModeSet(invisiblemode) && !inside && !source->HasPrivPermission("users/auspex"))
				continue;
		}

		SendWhoLine(source, parameters, i->second, u, data);
	}
}

template<typename T>
void CommandWho::WhoUsers(User *source, const std::vector<std::string>& parameters, T& users, WhoData &data)
{
	for (typename T::iterator it = users.begin(); it != users.end(); ++it)
	{
		User *u = GetUser(it);

		if (Match(source, u, data))
		{
			if (!source->SharesChannelWith(u))
			{
				if (data.wildcards && u->IsModeSet(invisiblemode) && !source->HasPrivPermission("users/auspex"))
					continue;
			}

			SendWhoLine(source, parameters, NULL, u, data);
		}
	}
}

bool CommandWho::CanView(Channel* chan, User* user)
{
	if (!user || !chan)
		return false;

	/* Bug #383 - moved higher up the list, because if we are in the channel
	 * we can see all its users
	 */
	if (chan->HasUser(user))
		return true;
	/* Opers see all */
	if (user->HasPrivPermission("users/auspex"))
		return true;
	/* Cant see inside a +s or a +p channel unless we are a member (see above) */
	else if (!chan->IsModeSet(secretmode) && !chan->IsModeSet(privatemode))
		return true;

	return false;
}

void CommandWho::SendWhoLine(User* user, const std::vector<std::string>& parms, Membership* memb, User* u, WhoData &data)
{
	std::string wholine = data.initial;

	if (!memb)
		memb = GetFirstVisibleChannel(u);
	Channel* ch = memb ? memb->chan : NULL;

	if (!data.whox)
	{
		bool showrealhost = data.flags['h'] && user->HasPrivPermission("users/auspex");

		wholine += (ch ? ch->name : "*") + " " + u->ident + " " +
			(showrealhost ? u->host : u->dhost) + " ";
		if (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
			wholine.append(ServerInstance->Config->HideWhoisServer);
		else
			wholine.append(u->server->GetName());

		wholine.append(" " + u->nick + " ");

		/* away? */
		if (u->IsAway())
			wholine.append("G");
		else
			wholine.append("H");

		/* oper? */
		if (u->IsOper())
			wholine.push_back('*');

		if (memb && memb->GetPrefixChar())
			wholine.append("" + memb->GetPrefixChar());

		wholine.append(" :0 " + u->fullname);
	}
	else
	{
		if (data.whox_flags['t'])
			wholine += (data.querytext.empty() ? "0" : data.querytext.substr(0, 3)) + " ";
		if (data.whox_flags['c'])
			wholine += (ch ? ch->name : "*") + " ";
		if (data.whox_flags['u'])
			wholine += u->ident + " ";
		if (data.whox_flags['i'])
		{
			if (user == u || user->HasPrivPermission("users/auspex"))
				wholine += u->GetIPString() + " ";
			else
				wholine += "255.255.255.255 ";
		}
		if (data.whox_flags['h'])
		{
			bool showrealhost = data.flags['h'] && user->HasPrivPermission("users/auspex");
			wholine += (showrealhost ? u->host : u->dhost) + " ";
		}
		if (data.whox_flags['s'])
		{
			if (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
				wholine += ServerInstance->Config->HideWhoisServer + " ";
			else
				wholine += u->server->GetName() + " ";
		}
		if (data.whox_flags['n'])
			wholine += u->nick + " ";
		if (data.whox_flags['f'])
		{
			if (u->IsAway())
				wholine.append("G");
			else
				wholine.append("H");
			if (u->IsOper())
				wholine.append("*");
			if (memb && memb->GetPrefixChar())
				wholine.append("" + memb->GetPrefixChar());
			wholine.append(" ");
		}
		if (data.whox_flags['d']) // Hops
			wholine += "0 ";
		if (data.whox_flags['l']) // Idle
			wholine += "0 ";
		if (data.whox_flags['a'])
		{
			const AccountExtItem* accountext = GetAccountExtItem();
			std::string *account = accountext ? accountext->get(u) : NULL;
			wholine += (account ? *account : "0") + " ";
		}
		if (data.whox_flags['o'])
			wholine += (memb ? ConvToStr(memb->getRank()) : "n/a") + " ";
		if (data.whox_flags['r'])
			wholine += u->fullname;
	}

	FOREACH_MOD(OnSendWhoLine, (user, parms, u, memb, wholine));

	if (!wholine.empty())
		data.results.push_back(wholine);
}

CmdResult CommandWho::Handle(const std::vector<std::string>& parameters, User *user)
{
	WhoData data;

	data.initial = "352 " + user->nick + " ";

	if (parameters.size() > 2)
		// If we ar given 3 or more parameters, the mask becomes parameters 2+, and parameter 0 is ignored
		for (unsigned i = 2; i < parameters.size(); ++i)
		{
			if (!data.matchtext.empty())
				data.matchtext += " ";
			data.matchtext += parameters[i];
		}
	else
		data.matchtext = parameters[0];

	/* Change '0' into '*' so the wildcard matcher can grok it */
	if (data.matchtext == "0")
		data.matchtext = "*";

	// WHO flags count as a wildcard
	data.wildcards = ((parameters.size() > 1) || (data.matchtext.find_first_of("*?.") != std::string::npos));

	if (parameters.size() > 1)
	{
		const std::string &flags = parameters[1];

		for (unsigned i = 0; i < flags.size(); ++i)
			switch (flags[i])
			{
				default:
					data.flags[static_cast<unsigned char>(flags[i])] = true;
					break;
				case '%':
					/* WHOX */
					data.whox = true;
					for (++i; i < flags.size(); ++i)
						switch (flags[i])
						{
							default:
								data.whox_flags[static_cast<unsigned char>(flags[i])] = true;
								break;
							case ',':
								data.querytext = flags.substr(i + 1);
								i = flags.size(); /* End loop */
						}
			}
	}


	/* who on a channel? */
	Channel* ch = ServerInstance->FindChan(data.matchtext);

	if (ch)
	{
		WhoChannel(user, parameters, ch, data);
	}
	else
	{
		/* Match against wildcard of nick, server or host */

		/* If we only want to match against opers, we only have to iterate the oper list */
		if (data.flags['o'])
			WhoUsers(user, parameters, ServerInstance->Users->all_opers, data);
		else
			WhoUsers(user, parameters, *ServerInstance->Users->clientlist, data);
	}

	/* Send the results out */
	for (std::vector<std::string>::const_iterator n = data.results.begin(); n != data.results.end(); n++)
		user->WriteServ(*n);
	user->WriteNumeric(RPL_ENDOFWHO, "%s :End of /WHO list.", data.matchtext.empty() ? "*" : data.matchtext.c_str());

	// Penalize the user a bit for large queries
	// (add one unit of penalty per 200 results)
	if (IS_LOCAL(user))
		IS_LOCAL(user)->CommandFloodPenalty += data.results.size() * 5;
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWho)
