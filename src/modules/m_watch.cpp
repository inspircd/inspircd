/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


/*
 * Okay, it's nice that this was documented and all, but I at least understood very little
 * of it, so I'm going to attempt to explain the data structures in here a bit more.
 *
 * For efficiency, many data structures are kept.
 *
 * The first is a global list `watchentries':
 *	hash_map<irc::string, std::deque<User*> >
 *
 * That is, if nick 'w00t' is being watched by user pointer 'Brain' and 'Om', <w00t, (Brain, Om)>
 * will be in the watchentries list.
 *
 * The second is that each user has a per-user data structure attached to their user record via Extensible:
 *	std::map<irc::string, std::string> watchlist;
 * So, in the above example with w00t watched by Brain and Om, we'd have:
 * 	Brain-
 * 	      `- w00t
 * 	Om-
 * 	   `- w00t
 *
 * Hopefully this helps any brave soul that ventures into this file other than me. :-)
 *		-- w00t (mar 30, 2008)
 */


/* This module has been refactored to provide a very efficient (in terms of cpu time)
 * implementation of /WATCH.
 *
 * To improve the efficiency of watch, many lists are kept. The first primary list is
 * a hash_map of who's being watched by who. For example:
 *
 * KEY: Brain   --->  Watched by:  Boo, w00t, Om
 * KEY: Boo     --->  Watched by:  Brain, w00t
 *
 * This is used when we want to tell all the users that are watching someone that
 * they are now available or no longer available. For example, if the hash was
 * populated as shown above, then when Brain signs on, messages are sent to Boo, w00t
 * and Om by reading their 'watched by' list. When this occurs, their online status
 * in each of these users lists (see below) is also updated.
 *
 * Each user also has a seperate (smaller) map attached to their User whilst they
 * have any watch entries, which is managed by class Extensible. When they add or remove
 * a watch entry from their list, it is inserted here, as well as the main list being
 * maintained. This map also contains the user's online status. For users that are
 * offline, the key points at an empty string, and for users that are online, the key
 * points at a string containing "users-ident users-host users-signon-time". This is
 * stored in this manner so that we don't have to FindUser() to fetch this info, the
 * users signon can populate the field for us.
 *
 * For example, going again on the example above, this would be w00t's watchlist:
 *
 * KEY: Boo    --->  Status: "Boo brains.sexy.babe 535342348"
 * KEY: Brain  --->  Status: ""
 *
 * In this list we can see that Boo is online, and Brain is offline. We can then
 * use this list for 'WATCH L', and 'WATCH S' can be implemented as a combination
 * of the above two data structures, with minimum CPU penalty for doing so.
 *
 * In short, the least efficient this ever gets is O(n), and thats only because
 * there are parts that *must* loop (e.g. telling all users that are watching a
 * nick that the user online), however this is a *major* improvement over the
 * 1.0 implementation, which in places had O(n^n) and worse in it, because this
 * implementation scales based upon the sizes of the watch entries, whereas the
 * old system would scale (or not as the case may be) according to the total number
 * of users using WATCH.
 */

typedef TR1NS::unordered_map<irc::string, std::deque<User*>, irc::hash> watchentries;
typedef std::map<irc::string, std::string> watchlist;

/* Who's watching each nickname.
 * NOTE: We do NOT iterate this to display a user's WATCH list!
 * See the comments above!
 */
watchentries* whos_watching_me;

class CommandSVSWatch : public Command
{
 public:
	CommandSVSWatch(Module* Creator) : Command(Creator,"SVSWATCH", 2)
	{
		syntax = "<target> [C|L|S]|[+|-<nick>]";
		TRANSLATE2(TR_NICK, TR_TEXT); /* we watch for a nick. not a UID. */
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (!user->server->IsULine())
			return CMD_FAILURE;

		User *u = ServerInstance->FindNick(parameters[0]);
		if (!u)
			return CMD_FAILURE;

		if (IS_LOCAL(u))
		{
			ServerInstance->Parser.CallHandler("WATCH", parameters, u);
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		if (target)
			return ROUTE_OPT_UCAST(target->server);
		return ROUTE_LOCALONLY;
	}
};

/** Handle /WATCH
 */
class CommandWatch : public Command
{
	unsigned int& MAX_WATCH;
 public:
	SimpleExtItem<watchlist> ext;
	CmdResult remove_watch(User* user, const char* nick)
	{
		// removing an item from the list
		if (!ServerInstance->IsNick(nick))
		{
			user->WriteNumeric(942, "%s :Invalid nickname", nick);
			return CMD_FAILURE;
		}

		watchlist* wl = ext.get(user);
		if (wl)
		{
			/* Yup, is on my list */
			watchlist::iterator n = wl->find(nick);

			if (n != wl->end())
			{
				if (!n->second.empty())
					user->WriteNumeric(602, "%s %s :stopped watching", n->first.c_str(), n->second.c_str());
				else
					user->WriteNumeric(602, "%s * * 0 :stopped watching", nick);

				wl->erase(n);
			}

			if (wl->empty())
			{
				ext.unset(user);
			}

			watchentries::iterator x = whos_watching_me->find(nick);
			if (x != whos_watching_me->end())
			{
				/* People are watching this user, am i one of them? */
				std::deque<User*>::iterator n2 = std::find(x->second.begin(), x->second.end(), user);
				if (n2 != x->second.end())
					/* I'm no longer watching you... */
					x->second.erase(n2);

				if (x->second.empty())
					/* nobody else is, either. */
					whos_watching_me->erase(nick);
			}
		}

		return CMD_SUCCESS;
	}

	CmdResult add_watch(User* user, const char* nick)
	{
		if (!ServerInstance->IsNick(nick))
		{
			user->WriteNumeric(942, "%s :Invalid nickname", nick);
			return CMD_FAILURE;
		}

		watchlist* wl = ext.get(user);
		if (!wl)
		{
			wl = new watchlist();
			ext.set(user, wl);
		}

		if (wl->size() >= MAX_WATCH)
		{
			user->WriteNumeric(512, "%s :Too many WATCH entries", nick);
			return CMD_FAILURE;
		}

		watchlist::iterator n = wl->find(nick);
		if (n == wl->end())
		{
			/* Don't already have the user on my watch list, proceed */
			watchentries::iterator x = whos_watching_me->find(nick);
			if (x != whos_watching_me->end())
			{
				/* People are watching this user, add myself */
				x->second.push_back(user);
			}
			else
			{
				std::deque<User*> newlist;
				newlist.push_back(user);
				(*(whos_watching_me))[nick] = newlist;
			}

			User* target = ServerInstance->FindNick(nick);
			if ((target) && (target->registered == REG_ALL))
			{
				(*wl)[nick] = std::string(target->ident).append(" ").append(target->dhost).append(" ").append(ConvToStr(target->age));
				user->WriteNumeric(604, "%s %s :is online", nick, (*wl)[nick].c_str());
				if (target->IsAway())
				{
					user->WriteNumeric(609, "%s %s %s %lu :is away", target->nick.c_str(), target->ident.c_str(), target->dhost.c_str(), (unsigned long) target->awaytime);
				}
			}
			else
			{
				(*wl)[nick].clear();
				user->WriteNumeric(605, "%s * * 0 :is offline", nick);
			}
		}

		return CMD_SUCCESS;
	}

	CommandWatch(Module* parent, unsigned int &maxwatch) : Command(parent,"WATCH", 0), MAX_WATCH(maxwatch), ext("watchlist", ExtensionItem::EXT_USER, parent)
	{
		syntax = "[C|L|S]|[+|-<nick>]";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		if (parameters.empty())
		{
			watchlist* wl = ext.get(user);
			if (wl)
			{
				for (watchlist::iterator q = wl->begin(); q != wl->end(); q++)
				{
					if (!q->second.empty())
						user->WriteNumeric(604, "%s %s :is online", q->first.c_str(), q->second.c_str());
				}
			}
			user->WriteNumeric(607, ":End of WATCH list");
		}
		else if (parameters.size() > 0)
		{
			for (int x = 0; x < (int)parameters.size(); x++)
			{
				const char *nick = parameters[x].c_str();
				if (!strcasecmp(nick,"C"))
				{
					// watch clear
					watchlist* wl = ext.get(user);
					if (wl)
					{
						for (watchlist::iterator i = wl->begin(); i != wl->end(); i++)
						{
							watchentries::iterator i2 = whos_watching_me->find(i->first);
							if (i2 != whos_watching_me->end())
							{
								/* People are watching this user, am i one of them? */
								std::deque<User*>::iterator n = std::find(i2->second.begin(), i2->second.end(), user);
								if (n != i2->second.end())
									/* I'm no longer watching you... */
									i2->second.erase(n);

								if (i2->second.empty())
									/* nobody else is, either. */
									whos_watching_me->erase(i2);
							}
						}

						ext.unset(user);
					}
				}
				else if (!strcasecmp(nick,"L"))
				{
					watchlist* wl = ext.get(user);
					if (wl)
					{
						for (watchlist::iterator q = wl->begin(); q != wl->end(); q++)
						{
							User* targ = ServerInstance->FindNick(q->first.c_str());
							if (targ && !q->second.empty())
							{
								user->WriteNumeric(604, "%s %s :is online", q->first.c_str(), q->second.c_str());
								if (targ->IsAway())
								{
									user->WriteNumeric(609, "%s %s %s %lu :is away", targ->nick.c_str(), targ->ident.c_str(), targ->dhost.c_str(), (unsigned long) targ->awaytime);
								}
							}
							else
								user->WriteNumeric(605, "%s * * 0 :is offline", q->first.c_str());
						}
					}
					user->WriteNumeric(607, ":End of WATCH list");
				}
				else if (!strcasecmp(nick,"S"))
				{
					watchlist* wl = ext.get(user);
					int you_have = 0;
					int youre_on = 0;
					std::string list;

					if (wl)
					{
						for (watchlist::iterator q = wl->begin(); q != wl->end(); q++)
							list.append(q->first.c_str()).append(" ");
						you_have = wl->size();
					}

					watchentries::iterator i2 = whos_watching_me->find(user->nick.c_str());
					if (i2 != whos_watching_me->end())
						youre_on = i2->second.size();

					user->WriteNumeric(603, ":You have %d and are on %d WATCH entries", you_have, youre_on);
					user->WriteNumeric(606, ":%s", list.c_str());
					user->WriteNumeric(607, ":End of WATCH S");
				}
				else if (nick[0] == '-')
				{
					nick++;
					remove_watch(user, nick);
				}
				else if (nick[0] == '+')
				{
					nick++;
					add_watch(user, nick);
				}
			}
		}
		return CMD_SUCCESS;
	}
};

class Modulewatch : public Module
{
	unsigned int maxwatch;
	CommandWatch cmdw;
	CommandSVSWatch sw;

 public:
	Modulewatch()
		: maxwatch(32), cmdw(this, maxwatch), sw(this)
	{
		whos_watching_me = new watchentries();
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		maxwatch = ServerInstance->Config->ConfValue("watch")->getInt("maxentries", 32);
		if (!maxwatch)
			maxwatch = 32;
	}

	ModResult OnSetAway(User *user, const std::string &awaymsg) CXX11_OVERRIDE
	{
		std::string numeric;
		int inum;

		if (awaymsg.empty())
		{
			numeric = user->nick + " " + user->ident + " " + user->dhost + " " + ConvToStr(ServerInstance->Time()) + " :is no longer away";
			inum = 599;
		}
		else
		{
			numeric = user->nick + " " + user->ident + " " + user->dhost + " " + ConvToStr(ServerInstance->Time()) + " :" + awaymsg;
			inum = 598;
		}

		watchentries::iterator x = whos_watching_me->find(user->nick.c_str());
		if (x != whos_watching_me->end())
		{
			for (std::deque<User*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
				(*n)->WriteNumeric(inum, numeric);
			}
		}

		return MOD_RES_PASSTHRU;
	}

	void OnUserQuit(User* user, const std::string &reason, const std::string &oper_message) CXX11_OVERRIDE
	{
		watchentries::iterator x = whos_watching_me->find(user->nick.c_str());
		if (x != whos_watching_me->end())
		{
			for (std::deque<User*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
				(*n)->WriteNumeric(601, "%s %s %s %lu :went offline", user->nick.c_str(), user->ident.c_str(), user->dhost.c_str(), (unsigned long) ServerInstance->Time());

				watchlist* wl = cmdw.ext.get(*n);
				if (wl)
					/* We were on somebody's notify list, set ourselves offline */
					(*wl)[user->nick.c_str()].clear();
			}
		}

		/* Now im quitting, if i have a notify list, im no longer watching anyone */
		watchlist* wl = cmdw.ext.get(user);
		if (wl)
		{
			/* Iterate every user on my watch list, and take me out of the whos_watching_me map for each one we're watching */
			for (watchlist::iterator i = wl->begin(); i != wl->end(); i++)
			{
				watchentries::iterator i2 = whos_watching_me->find(i->first);
				if (i2 != whos_watching_me->end())
				{
						/* People are watching this user, am i one of them? */
						std::deque<User*>::iterator n = std::find(i2->second.begin(), i2->second.end(), user);
						if (n != i2->second.end())
							/* I'm no longer watching you... */
							i2->second.erase(n);

						if (i2->second.empty())
							/* and nobody else is, either. */
							whos_watching_me->erase(i2);
				}
			}
		}
	}

	void OnGarbageCollect()
	{
		watchentries* old_watch = whos_watching_me;
		whos_watching_me = new watchentries();

		for (watchentries::const_iterator n = old_watch->begin(); n != old_watch->end(); n++)
			whos_watching_me->insert(*n);

		delete old_watch;
	}

	void OnPostConnect(User* user) CXX11_OVERRIDE
	{
		watchentries::iterator x = whos_watching_me->find(user->nick.c_str());
		if (x != whos_watching_me->end())
		{
			for (std::deque<User*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
				(*n)->WriteNumeric(600, "%s %s %s %lu :arrived online", user->nick.c_str(), user->ident.c_str(), user->dhost.c_str(), (unsigned long) user->age);

				watchlist* wl = cmdw.ext.get(*n);
				if (wl)
					/* We were on somebody's notify list, set ourselves online */
					(*wl)[user->nick.c_str()] = std::string(user->ident).append(" ").append(user->dhost).append(" ").append(ConvToStr(user->age));
			}
		}
	}

	void OnUserPostNick(User* user, const std::string &oldnick) CXX11_OVERRIDE
	{
		watchentries::iterator new_offline = whos_watching_me->find(oldnick.c_str());
		watchentries::iterator new_online = whos_watching_me->find(user->nick.c_str());

		if (new_offline != whos_watching_me->end())
		{
			for (std::deque<User*>::iterator n = new_offline->second.begin(); n != new_offline->second.end(); n++)
			{
				watchlist* wl = cmdw.ext.get(*n);
				if (wl)
				{
					(*n)->WriteNumeric(601, "%s %s %s %lu :went offline", oldnick.c_str(), user->ident.c_str(), user->dhost.c_str(), (unsigned long) user->age);
					(*wl)[oldnick.c_str()].clear();
				}
			}
		}

		if (new_online != whos_watching_me->end())
		{
			for (std::deque<User*>::iterator n = new_online->second.begin(); n != new_online->second.end(); n++)
			{
				watchlist* wl = cmdw.ext.get(*n);
				if (wl)
				{
					(*wl)[user->nick.c_str()] = std::string(user->ident).append(" ").append(user->dhost).append(" ").append(ConvToStr(user->age));
					(*n)->WriteNumeric(600, "%s %s :arrived online", user->nick.c_str(), (*wl)[user->nick.c_str()].c_str());
				}
			}
		}
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["WATCH"] = ConvToStr(maxwatch);
	}

	~Modulewatch()
	{
		delete whos_watching_me;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the /WATCH command", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(Modulewatch)
