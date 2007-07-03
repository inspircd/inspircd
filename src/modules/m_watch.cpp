/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"

/* $ModDesc: Provides support for the /WATCH command */

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
 * Each user also has a seperate (smaller) map attached to their userrec whilst they
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

/*
 * Before you start screaming, this definition is only used here, so moving it to a header is pointless.
 * Yes, it's horrid. Blame cl for being different. -- w00t
 */
#ifdef WINDOWS
typedef nspace::hash_map<irc::string, std::deque<userrec*>, nspace::hash_compare<irc::string, less<irc::string> > > watchentries;
#else
typedef nspace::hash_map<irc::string, std::deque<userrec*>, nspace::hash<irc::string> > watchentries;
#endif
typedef std::map<irc::string, std::string> watchlist;

/* Who's watching each nickname.
 * NOTE: We do NOT iterate this to display a user's WATCH list!
 * See the comments above!
 */
watchentries* whos_watching_me;

/** Handle /WATCH
 */
class cmd_watch : public command_t
{
	unsigned int& MAX_WATCH;
 public:
	CmdResult remove_watch(userrec* user, const char* nick)
	{
		// removing an item from the list
		if (!ServerInstance->IsNick(nick))
		{
			user->WriteServ("942 %s %s :Invalid nickname", user->nick, nick);
			return CMD_FAILURE;
		}

		watchlist* wl;
		if (user->GetExt("watchlist", wl))
		{
			/* Yup, is on my list */
			watchlist::iterator n = wl->find(nick);
			if (n != wl->end())
			{
				if (!n->second.empty())
					user->WriteServ("602 %s %s %s :stopped watching", user->nick, n->first.c_str(), n->second.c_str());
				else
					user->WriteServ("602 %s %s * * 0 :stopped watching", user->nick, nick);

				wl->erase(n);
			}

			if (!wl->size())
			{
				user->Shrink("watchlist");
				delete wl;
			}

			watchentries::iterator x = whos_watching_me->find(nick);
			if (x != whos_watching_me->end())
			{
				/* People are watching this user, am i one of them? */
				std::deque<userrec*>::iterator n = std::find(x->second.begin(), x->second.end(), user);
				if (n != x->second.end())
					/* I'm no longer watching you... */
					x->second.erase(n);

				if (!x->second.size())
					whos_watching_me->erase(nick);
			}
		}

		/* This might seem confusing, but we return CMD_FAILURE
		 * to indicate that this message shouldnt be routed across
		 * the network to other linked servers.
		 */
		return CMD_FAILURE;
	}

	CmdResult add_watch(userrec* user, const char* nick)
	{
		if (!ServerInstance->IsNick(nick))
		{
			user->WriteServ("942 %s %s :Invalid nickname",user->nick,nick);
			return CMD_FAILURE;
		}

		watchlist* wl;
		if (!user->GetExt("watchlist", wl))
		{
			wl = new watchlist();
			user->Extend("watchlist", wl);
		}

		if (wl->size() == MAX_WATCH)
		{
			user->WriteServ("512 %s %s :Too many WATCH entries", user->nick, nick);
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
				std::deque<userrec*> newlist;
				newlist.push_back(user);
				(*(whos_watching_me))[nick] = newlist;
			}

			userrec* target = ServerInstance->FindNick(nick);
			if (target)
			{
				if (target->Visibility && !target->Visibility->VisibleTo(user))
				{
					(*wl)[nick] = "";
					user->WriteServ("605 %s %s * * 0 :is offline",user->nick, nick);
					return CMD_FAILURE;
				}

				(*wl)[nick] = std::string(target->ident).append(" ").append(target->dhost).append(" ").append(ConvToStr(target->age));
				user->WriteServ("604 %s %s %s :is online",user->nick, nick, (*wl)[nick].c_str());
			}
			else
			{
				(*wl)[nick] = "";
				user->WriteServ("605 %s %s * * 0 :is offline",user->nick, nick);
			}
		}

		return CMD_FAILURE;
	}

	cmd_watch (InspIRCd* Instance, unsigned int &maxwatch) : command_t(Instance,"WATCH",0,0), MAX_WATCH(maxwatch)
	{
		this->source = "m_watch.so";
		syntax = "[C|L|S]|[+|-<nick>]";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (!pcnt)
		{
			watchlist* wl;
			if (user->GetExt("watchlist", wl))
			{
				for (watchlist::iterator q = wl->begin(); q != wl->end(); q++)
				{
					if (!q->second.empty())
						user->WriteServ("604 %s %s %s :is online", user->nick, q->first.c_str(), q->second.c_str());
				}
			}
			user->WriteServ("607 %s :End of WATCH list",user->nick);
		}
		else if (pcnt > 0)
		{
			for (int x = 0; x < pcnt; x++)
			{
				const char *nick = parameters[x];
				if (!strcasecmp(nick,"C"))
				{
					// watch clear
					watchlist* wl;
					if (user->GetExt("watchlist", wl))
					{
						for (watchlist::iterator i = wl->begin(); i != wl->end(); i++)
						{
							watchentries::iterator x = whos_watching_me->find(i->first);
							if (x != whos_watching_me->end())
							{
								/* People are watching this user, am i one of them? */
								std::deque<userrec*>::iterator n = std::find(x->second.begin(), x->second.end(), user);
								if (n != x->second.end())
									/* I'm no longer watching you... */
									x->second.erase(n);

								if (!x->second.size())
									whos_watching_me->erase(user->nick);
							}
						}

						delete wl;
						user->Shrink("watchlist");
					}
				}
				else if (!strcasecmp(nick,"L"))
				{
					watchlist* wl;
					if (user->GetExt("watchlist", wl))
					{
						for (watchlist::iterator q = wl->begin(); q != wl->end(); q++)
						{
							if (!q->second.empty())
								user->WriteServ("604 %s %s %s :is online", user->nick, q->first.c_str(), q->second.c_str());
							else
								user->WriteServ("605 %s %s * * 0 :is offline", user->nick, q->first.c_str());
						}
					}
					user->WriteServ("607 %s :End of WATCH list",user->nick);
				}
				else if (!strcasecmp(nick,"S"))
				{
					watchlist* wl;
					int you_have = 0;
					int youre_on = 0;
					std::string list;

					if (user->GetExt("watchlist", wl))
					{
						for (watchlist::iterator q = wl->begin(); q != wl->end(); q++)
							list.append(q->first.c_str()).append(" ");
						you_have = wl->size();
					}

					watchentries::iterator x = whos_watching_me->find(user->nick);
					if (x != whos_watching_me->end())
						youre_on = x->second.size();

					user->WriteServ("603 %s :You have %d and are on %d WATCH entries", user->nick, you_have, youre_on);
					user->WriteServ("606 %s :%s",user->nick, list.c_str());
					user->WriteServ("607 %s :End of WATCH S",user->nick);
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
		/* So that spanningtree doesnt pass the WATCH commands to the network! */
		return CMD_FAILURE;
	}
};

class Modulewatch : public Module
{
	cmd_watch* mycommand;
	unsigned int maxwatch;
 public:

	Modulewatch(InspIRCd* Me)
		: Module(Me), maxwatch(32)
	{
		OnRehash(NULL, "");
		whos_watching_me = new watchentries();
		mycommand = new cmd_watch(ServerInstance, maxwatch);
		ServerInstance->AddCommand(mycommand);
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		maxwatch = Conf.ReadInteger("watch", "maxentries", 0, true);
		if (!maxwatch)
			maxwatch = 32;
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnGarbageCollect] = List[I_OnCleanup] = List[I_OnUserQuit] = List[I_OnPostConnect] = List[I_OnUserPostNick] = List[I_On005Numeric] = 1;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason, const std::string &oper_message)
	{
		watchentries::iterator x = whos_watching_me->find(user->nick);
		if (x != whos_watching_me->end())
		{
			for (std::deque<userrec*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
				if (!user->Visibility || user->Visibility->VisibleTo(user))
					(*n)->WriteServ("601 %s %s %s %s %lu :went offline", (*n)->nick ,user->nick, user->ident, user->dhost, ServerInstance->Time());

				watchlist* wl;
				if ((*n)->GetExt("watchlist", wl))
					/* We were on somebody's notify list, set ourselves offline */
					(*wl)[user->nick] = "";
			}
		}

		/* Now im quitting, if i have a notify list, im no longer watching anyone */
		watchlist* wl;
		if (user->GetExt("watchlist", wl))
		{
			/* Iterate every user on my watch list, and take me out of the whos_watching_me map for each one we're watching */
			for (watchlist::iterator i = wl->begin(); i != wl->end(); i++)
			{
				watchentries::iterator x = whos_watching_me->find(i->first);
				if (x != whos_watching_me->end())
				{
						/* People are watching this user, am i one of them? */
						std::deque<userrec*>::iterator n = std::find(x->second.begin(), x->second.end(), user);
						if (n != x->second.end())
							/* I'm no longer watching you... */
							x->second.erase(n);
	
						if (!x->second.size())
							whos_watching_me->erase(user->nick);
				}
			}

			/* User's quitting, we're done with this. */
			delete wl;
		}
	}

	virtual void OnGarbageCollect()
	{
		watchentries* old_watch = whos_watching_me;
		whos_watching_me = new watchentries();

		for (watchentries::const_iterator n = old_watch->begin(); n != old_watch->end(); n++)
			whos_watching_me->insert(*n);

		delete old_watch;
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			watchlist* wl;
			userrec* user = (userrec*)item;

			if (user->GetExt("watchlist", wl))
			{
				user->Shrink("watchlist");
				delete wl;
			}
		}
	}

	virtual void OnPostConnect(userrec* user)
	{
		watchentries::iterator x = whos_watching_me->find(user->nick);
		if (x != whos_watching_me->end())
		{
			for (std::deque<userrec*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
				if (!user->Visibility || user->Visibility->VisibleTo(user))
					(*n)->WriteServ("600 %s %s %s %s %lu :arrived online", (*n)->nick, user->nick, user->ident, user->dhost, user->age);

				watchlist* wl;
				if ((*n)->GetExt("watchlist", wl))
					/* We were on somebody's notify list, set ourselves online */
					(*wl)[user->nick] = std::string(user->ident).append(" ").append(user->dhost).append(" ").append(ConvToStr(user->age));
			}
		}
	}

	virtual void OnUserPostNick(userrec* user, const std::string &oldnick)
	{
		watchentries::iterator new_online = whos_watching_me->find(user->nick);
		watchentries::iterator new_offline = whos_watching_me->find(assign(oldnick));

		if (new_online != whos_watching_me->end())
		{
			for (std::deque<userrec*>::iterator n = new_online->second.begin(); n != new_online->second.end(); n++)
			{
				watchlist* wl;
				if ((*n)->GetExt("watchlist", wl))
				{
					(*wl)[user->nick] = std::string(user->ident).append(" ").append(user->dhost).append(" ").append(ConvToStr(user->age));
					if (!user->Visibility || user->Visibility->VisibleTo(user))
						(*n)->WriteServ("600 %s %s %s :arrived online", (*n)->nick, user->nick, (*wl)[user->nick].c_str());
				}
			}
		}

		if (new_offline != whos_watching_me->end())
		{
			for (std::deque<userrec*>::iterator n = new_offline->second.begin(); n != new_offline->second.end(); n++)
			{
				watchlist* wl;
				if ((*n)->GetExt("watchlist", wl))
				{
					if (!user->Visibility || user->Visibility->VisibleTo(user))
	 					(*n)->WriteServ("601 %s %s %s %s %lu :went offline", (*n)->nick, oldnick.c_str(), user->ident, user->dhost, user->age);
					(*wl)[oldnick.c_str()] = "";
				}
			}
		}
	}	

	virtual void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " WATCH=" + ConvToStr(maxwatch);
	}
	
	virtual ~Modulewatch()
	{
		delete whos_watching_me;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(Modulewatch)

