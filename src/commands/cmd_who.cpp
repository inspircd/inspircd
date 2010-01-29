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

 public:
	/** Constructor for who.
	 */
	CommandWho ( Module* parent) : Command(parent,"WHO", 1) {
		syntax = "<server>|<nickname>|<channel>|<realname>|<host>|0 [ohurmMiaplf]";
	}
	void SendWhoLine(User* user, const std::string &initial, Channel* ch, User* u, std::vector<std::string> &whoresults);
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	bool whomatch(User* cuser, User* user, const char* matchtext);
};


static const std::string star = "*";

static const std::string& get_first_visible_channel(User *u)
{
	UCListIter i = u->chans.begin();
	while (i != u->chans.end())
	{
		Channel* c = *i++;
		if (!c->IsModeSet('s'))
			return c->name;
	}

	return star;
}

bool CommandWho::whomatch(User* cuser, User* user, const char* matchtext)
{
	bool match = false;
	bool positive = false;

	if (user->registered != REG_ALL)
		return false;

	if (opt_local && !IS_LOCAL(user))
		return false;
	else if (opt_far && IS_LOCAL(user))
		return false;

	if (opt_mode)
	{
		for (const char* n = matchtext; *n; n++)
		{
			if (*n == '+')
			{
				positive = true;
				continue;
			}
			else if (*n == '-')
			{
				positive = false;
				continue;
			}
			if (user->IsModeSet(*n) != positive)
				return false;
		}
		return true;
	}
	else
	{
		/*
		 * This was previously one awesome pile of ugly nested if, when really, it didn't need
		 * to be, since only one condition was ever checked, a chained if works just fine.
		 * -- w00t
		 */
		if (opt_metadata)
		{
			match = false;
			const Extensible::ExtensibleStore& list = user->GetExtList();
			for(Extensible::ExtensibleStore::const_iterator i = list.begin(); i != list.end(); ++i)
				if (InspIRCd::Match(i->first->name, matchtext))
					match = true;
		}
		else if (opt_realname)
			match = InspIRCd::Match(user->fullname, matchtext);
		else if (opt_showrealhost)
			match = InspIRCd::Match(user->host, matchtext, ascii_case_insensitive_map);
		else if (opt_ident)
			match = InspIRCd::Match(user->ident, matchtext, ascii_case_insensitive_map);
		else if (opt_port)
		{
			irc::portparser portrange(matchtext, false);
			long portno = -1;
			while ((portno = portrange.GetToken()))
				if (IS_LOCAL(user) && portno == IS_LOCAL(user)->GetServerPort())
				{
					match = true;
					break;
				}
		}
		else if (opt_away)
			match = InspIRCd::Match(user->awaymsg, matchtext);
		else if (opt_time)
		{
			long seconds = ServerInstance->Duration(matchtext);

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
			match = InspIRCd::Match(user->dhost, matchtext, ascii_case_insensitive_map);

		if (!match)
			match = InspIRCd::Match(user->nick, matchtext);

		/* Don't allow server name matches if HideWhoisServer is enabled, unless the command user has the priv */
		if (!match && (ServerInstance->Config->HideWhoisServer.empty() || cuser->HasPrivPermission("users/auspex")))
			match = InspIRCd::Match(user->server, matchtext);

		return match;
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
	else if (!chan->IsModeSet('s') && !chan->IsModeSet('p'))
		return true;

	return false;
}

void CommandWho::SendWhoLine(User* user, const std::string &initial, Channel* ch, User* u, std::vector<std::string> &whoresults)
{
	const std::string& lcn = get_first_visible_channel(u);
	Channel* chlast = ServerInstance->FindChan(lcn);

	std::string wholine =	initial + (ch ? ch->name : lcn) + " " + u->ident + " " + (opt_showrealhost ? u->host : u->dhost) + " " +
				((!ServerInstance->Config->HideWhoisServer.empty() && !user->HasPrivPermission("servers/auspex")) ? ServerInstance->Config->HideWhoisServer : u->server) +
				" " + u->nick + " ";

	/* away? */
	if (IS_AWAY(u))
	{
		wholine.append("G");
	}
	else
	{
		wholine.append("H");
	}

	/* oper? */
	if (IS_OPER(u))
	{
		wholine.append("*");
	}

	wholine = wholine + (ch ? ch->GetPrefixChar(u) : (chlast ? chlast->GetPrefixChar(u) : "")) + " :0 " + u->fullname;

	FOREACH_MOD(I_OnSendWhoLine, OnSendWhoLine(user, u, ch, wholine));

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

	Channel *ch = NULL;
	std::vector<std::string> whoresults;
	std::string initial = "352 " + std::string(user->nick) + " ";

	char matchtext[MAXBUF];
	bool usingwildcards = false;

	/* Change '0' into '*' so the wildcard matcher can grok it */
	if (parameters[0] == "0")
		strlcpy(matchtext, "*", MAXBUF);
	else
		strlcpy(matchtext, parameters[0].c_str(), MAXBUF);

	for (const char* check = matchtext; *check; check++)
	{
		if (*check == '*' || *check == '?')
		{
			usingwildcards = true;
			break;
		}
	}

	if (parameters.size() > 1)
	{
		/* Fix for bug #444, WHO flags count as a wildcard */
		usingwildcards = true;

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
	ch = ServerInstance->FindChan(matchtext);

	if (ch)
	{
		if (CanView(ch,user))
		{
			bool inside = ch->HasUser(user);

			/* who on a channel. */
			const UserMembList *cu = ch->GetUsers();

			for (UserMembCIter i = cu->begin(); i != cu->end(); i++)
			{
				/* None of this applies if we WHO ourselves */
				if (user != i->first)
				{
					/* opers only, please */
					if (opt_viewopersonly && !IS_OPER(i->first))
						continue;

					/* If we're not inside the channel, hide +i users */
					if (i->first->IsModeSet('i') && !inside && !user->HasPrivPermission("users/auspex"))
						continue;
				}

				SendWhoLine(user, initial, ch, i->first, whoresults);
			}
		}
	}
	else
	{
		/* Match against wildcard of nick, server or host */
		if (opt_viewopersonly)
		{
			/* Showing only opers */
			for (std::list<User*>::iterator i = ServerInstance->Users->all_opers.begin(); i != ServerInstance->Users->all_opers.end(); i++)
			{
				User* oper = *i;

				if (whomatch(user, oper, matchtext))
				{
					if (!user->SharesChannelWith(oper))
					{
						if (usingwildcards && (!oper->IsModeSet('i')) && (!user->HasPrivPermission("users/auspex")))
							continue;
					}

					SendWhoLine(user, initial, NULL, oper, whoresults);
				}
			}
		}
		else
		{
			for (user_hash::iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); i++)
			{
				if (whomatch(user, i->second, matchtext))
				{
					if (!user->SharesChannelWith(i->second))
					{
						if (usingwildcards && (i->second->IsModeSet('i')) && (!user->HasPrivPermission("users/auspex")))
							continue;
					}

					SendWhoLine(user, initial, NULL, i->second, whoresults);
				}
			}
		}
	}
	/* Send the results out */
	for (std::vector<std::string>::const_iterator n = whoresults.begin(); n != whoresults.end(); n++)
		user->WriteServ(*n);
	user->WriteNumeric(315, "%s %s :End of /WHO list.",user->nick.c_str(), *parameters[0].c_str() ? parameters[0].c_str() : "*");

	// Penalize the user a bit for large queries
	// (add one unit of penalty per 200 results)
	if (IS_LOCAL(user))
		IS_LOCAL(user)->CommandFloodPenalty += whoresults.size() * 5;
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWho)
