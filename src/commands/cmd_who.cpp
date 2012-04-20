/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "commands/cmd_who.h"

static const std::string star = "*";

static const std::string& get_first_visible_channel(User *u)
{
	UCListIter i = u->chans.begin();
	if (i != u->chans.end())
	{
		if (!i->first->IsModeSet('s'))
			return i->first->name;
	}

	return star;
}

bool CommandWho::whomatch(User* cuser, User* user, const char* matchtext)
{
	bool match = false;
	bool positive = false;
	char* dummy = NULL;

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
			match = user->GetExt(matchtext, dummy);
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
				if (portno == user->GetPort())
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
		if (!match && (!*ServerInstance->Config->HideWhoisServer || cuser->HasPrivPermission("users/auspex")))
			match = InspIRCd::Match(user->server, matchtext);

		return match;
	}
}



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandWho(Instance);
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
	/* Not visible to this user */
	if (u->Visibility && !u->Visibility->VisibleTo(user))
		return;

	const std::string& lcn = get_first_visible_channel(u);
	Channel* chlast = ServerInstance->FindChan(lcn);

	std::string wholine =	initial + (ch ? ch->name : lcn) + " " + u->ident + " " + (opt_showrealhost ? u->host : u->dhost) + " " +
				((*ServerInstance->Config->HideWhoisServer && !user->HasPrivPermission("servers/auspex")) ? ServerInstance->Config->HideWhoisServer : u->server) +
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
	opt_unlimit = false;
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

	if (ServerInstance->FindServerName(matchtext))
		usingwildcards = true;

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
				case 'u':
					if (user->HasPrivPermission("users/auspex"))
						opt_unlimit = true;
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
					if (user->HasPrivPermission("users/auspex") || !*ServerInstance->Config->HideWhoisServer)
						opt_local = true;
					break;
				case 'f':
					if (user->HasPrivPermission("users/auspex") || !*ServerInstance->Config->HideWhoisServer)
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
			CUList *cu = ch->GetUsers();

			for (CUList::iterator i = cu->begin(); i != cu->end(); i++)
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
	if ((ServerInstance->Config->MaxWhoResults && (whoresults.size() <= (size_t)ServerInstance->Config->MaxWhoResults)) || opt_unlimit)
	{
		for (std::vector<std::string>::const_iterator n = whoresults.begin(); n != whoresults.end(); n++)
			user->WriteServ(*n);
		user->WriteNumeric(315, "%s %s :End of /WHO list.",user->nick.c_str(), *parameters[0].c_str() ? parameters[0].c_str() : "*");
		return CMD_SUCCESS;
	}
	else
	{
		/* BZZT! Too many results. */
		user->WriteNumeric(315, "%s %s :Too many results",user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}
}
