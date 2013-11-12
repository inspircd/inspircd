/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

/** Handle /WHO. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWho : public Command
{
	bool CanView(Channel* chan, User* user);
	bool opt_viewopersonly;
	bool opt_showrealhost;
	bool opt_realname;
	bool opt_mode;
	bool opt_ident;
	bool opt_metadata;
	bool opt_port;
	bool opt_away;
	bool opt_local;
	bool opt_far;
	bool opt_time;
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference invisiblemode;

	Channel* get_first_visible_channel(User *u)
	{
		UCListIter i = u->chans.begin();
		while (i != u->chans.end())
		{
			Channel* c = *i++;
			if (!c->IsModeSet(secretmode))
				return c;
		}
		return NULL;
	}

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

	void SendWhoLine(User* user, const std::vector<std::string>& parms, const std::string &initial, Channel* ch, User* u, std::vector<std::string> &whoresults);
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	bool whomatch(User* cuser, User* user, std::string &matchtext);
};

bool CommandWho::whomatch(User* cuser, User* user, std::string &matchtext)
{
	/*
	 * Test basic boolean logic first
	 */

	if (user->registered != REG_ALL)
		return false;
	if (opt_local && !IS_LOCAL(user))
		return false;
	if (opt_far && IS_LOCAL(user))
		return false;

	/*
	 * Test basic string parsing next
	 */

	if (opt_mode && user->IsModeSet(matchtext))
		return true;
	if (opt_time && user->age >= ServerInstance->Time - InspIRCd::Duration(matchtext))
		return true;

	if (opt_port)
	{
		irc::portparser portrange(matchtext.c_str(), false);
		long portno = -1;
		while ((portno = portrange.GetToken()))
			if (IS_LOCAL(user) && portno == IS_LOCAL(user)->GetServerPort())
				return true;
	}

	/*
	 * Optimise wildstrings and test them last
	 */

	InspIRCd::OptimiseWildStr(matchtext);

	if (opt_metadata)
	{
		const Extensible::ExtensibleStore& list = user->GetExtList();
		for(Extensible::ExtensibleStore::const_iterator i = list.begin(); i != list.end(); ++i)
			if (InspIRCd::Match(i->first->name, matchtext))
				return true;
	}

	return (opt_realname     && InspIRCd::Match(user->fullname, matchtext))
	    || (opt_showrealhost && InspIRCd::Match(user->host, matchtext, ascii_case_insensitive_map))
	    || (opt_ident        && InspIRCd::Match(user->ident, matchtext, ascii_case_insensitive_map))
	    || (opt_away         && InspIRCd::Match(user->awaymsg, matchtext))
	    || InspIRCd::Match(user->dhost, matchtext, ascii_case_insensitive_map)
	    || InspIRCd::Match(user->nick, matchtext)
	    || ((ServerInstance->Config->HideWhoisServer.empty() || cuser->HasPrivPermission("users/auspex")) && InspIRCd::Match(user->server, matchtext));
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

void CommandWho::SendWhoLine(User* user, const std::vector<std::string>& parms, const std::string &initial, Channel* ch, User* u, std::vector<std::string> &whoresults)
{
	if (!ch)
		ch = get_first_visible_channel(u);

	std::string wholine = initial + (ch ? ch->name : "*") + " " + u->ident + " " +
		(opt_showrealhost ? u->host : u->dhost) + " ";
	if (!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex"))
		wholine.append(ServerInstance->Config->HideWhoisServer);
	else
		wholine.append(u->server);

	wholine.append(" " + u->nick + " ");

	/* away? */
	if (u->IsAway())
	{
		wholine.append("G");
	}
	else
	{
		wholine.append("H");
	}

	/* oper? */
	if (u->IsOper())
	{
		wholine.push_back('*');
	}

	if (ch)
		wholine.append(ch->GetPrefixChar(u));

	wholine.append(" :0 " + u->fullname);

	FOREACH_MOD(OnSendWhoLine, (user, parms, u, wholine));

	if (!wholine.empty())
		whoresults.push_back(wholine);
}

CmdResult CommandWho::Handle (const std::vector<std::string>& parameters, User *user)
{
	/*
	 * XXX - RFC says:
	 *   The <name> passed to WHO is matched against users' host, server, real
	 *   name and nickname
	 * Currently, we support WHO #chan, WHO nick, WHO 0, WHO *, and the addition of a 'o' flag, as per RFC.
	 */

	/* WHO options */
	opt_viewopersonly = false;
	opt_showrealhost = false;
	opt_realname = false;
	opt_mode = false;
	opt_ident = false;
	opt_metadata = false;
	opt_port = false;
	opt_away = false;
	opt_local = false;
	opt_far = false;
	opt_time = false;

	std::vector<std::string> whoresults;
	std::string initial = "352 " + user->nick + " ";

	/* Change '0' into '*' so the wildcard matcher can grok it */
	std::string matchtext = parameters[0] == "0" ? "*" : parameters[0];

	bool usingwildcards = parameters.size() > 1 || matchtext.find_first_of("*?.") != std::string::npos;

	if (parameters.size() > 1)
	{
		for (std::string::const_iterator iter = parameters[1].begin(); iter != parameters[1].end(); ++iter)
		{
			switch (*iter)
			{
				case 'o':
					opt_viewopersonly = true;
					break;
				case 'h':
					if (user->HasPrivPermission("users/auspex"))
						opt_showrealhost = true;
					break;
				case 'r':
					opt_realname = true;
					break;
				case 'm':
					if (user->HasPrivPermission("users/auspex"))
						opt_mode = true;
					break;
				case 'M':
					if (user->HasPrivPermission("users/auspex"))
						opt_metadata = true;
					break;
				case 'i':
					opt_ident = true;
					break;
				case 'p':
					if (user->HasPrivPermission("users/auspex"))
						opt_port = true;
					break;
				case 'a':
					opt_away = true;
					break;
				case 'l':
					if (user->HasPrivPermission("users/auspex") || ServerInstance->Config->HideWhoisServer.empty())
						opt_local = true;
					break;
				case 'f':
					if (user->HasPrivPermission("users/auspex") || ServerInstance->Config->HideWhoisServer.empty())
						opt_far = true;
					break;
				case 't':
					opt_time = true;
					break;
			}
		}
	}

	/* who on a channel? */
	Channel* ch = ServerInstance->FindChan(matchtext);

	if (CanView(ch,user))
	{
		bool inside = ch->HasUser(user) || user->HasPrivPermission("users/auspex");

		/* Show matches when the sender's an oper, in the channel or the match is visible. */
		for (UserMembCIter i = ch->GetUsers()->begin(); i != ch->GetUsers()->end(); i++)
		{
			if (opt_viewopersonly && !i->first->IsOper())
				continue;
	
			if (inside || !i->first->IsModeSet(invisiblemode))
				SendWhoLine(user, parameters, initial, ch, i->first, whoresults);
		}
	}
	else if (!usingwildcards)
	{
		/* Show match when the sender queries an exact nickname */
		User *u = ServerInstance->FindNick(parameters[0]);
		if (u)
			SendWhoLine(user, parameters, initial, NULL, u, whoresults);
	}
	else if (opt_viewopersonly)
	{
		/* Show only opers */
		for (std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin(); i != ServerInstance->Users->all_opers.end(); i++)
			if (whomatch(user, *i, matchtext.c_str()))
				SendWhoLine(user, parameters, initial, NULL, *i, whoresults);
	}
	else if (!ch)
	{
		bool oper = user->HasPrivPermission("users/auspex");

		/* Show matches when the sender's an oper, shares a common channel or the match is visible. */
		for (user_hash::iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); i++)
			if (whomatch(user, i->second, matchtext.c_str()))
				if (oper || user->SharesChannelWith(i->second) || !i->second->IsModeSet(invisiblemode))
					SendWhoLine(user, parameters, initial, NULL, i->second, whoresults);
	}

	/* Send the results out */
	for (std::vector<std::string>::const_iterator n = whoresults.begin(); n != whoresults.end(); n++)
		user->WriteServ(*n);
	user->WriteNumeric(315, "%s %s :End of /WHO list.",user->nick.c_str(), *parameters[0].c_str() ? parameters[0].c_str() : "*");

	// Penalize the user a bit for large queries
	if (IS_LOCAL(user))
		IS_LOCAL(user)->CommandFloodPenalty += whoresults.size() * 5 + 1;
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWho)
