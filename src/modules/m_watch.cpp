/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *       E-mail:
 *<brain@chatspike.net>
 *   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "hashcomp.h"
#include "inspircd.h"

/* $ModDesc: Provides support for the /watch command */

/*       nickname             list of users watching the nick               */
typedef std::map<irc::string, std::deque<userrec*> >                        watchentries;

/*       nickname             'ident host signon', or empty if not online   */
typedef std::map<irc::string, std::string>                                  watchlist;

/* Whos watching each nickname */
watchentries whos_watching_me;

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

			watchentries::iterator x = whos_watching_me.find(nick);
			if (x != whos_watching_me.end())
			{
				/* People are watching this user, am i one of them? */
				std::deque<userrec*>::iterator n = std::find(x->second.begin(), x->second.end(), user);
				if (n != x->second.end())
					/* I'm no longer watching you... */
					x->second.erase(n);

				if (!x->second.size())
					whos_watching_me.erase(nick);
			}
		}

		return CMD_SUCCESS;
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
			user->WriteServ("942 %s %s :Too many WATCH entries", user->nick, nick);
			return CMD_FAILURE;
		}

		watchlist::iterator n = wl->find(nick);
		if (n == wl->end())
		{
			/* Don't already have the user on my watch list, proceed */
			watchentries::iterator x = whos_watching_me.find(nick);
			if (x != whos_watching_me.end())
			{
				/* People are watching this user, add myself */
				x->second.push_back(user);
			}
			else
			{
				std::deque<userrec*> newlist;
				newlist.push_back(user);
				whos_watching_me[nick] = newlist;
			}

			userrec* target = ServerInstance->FindNick(nick);
			if (target)
			{
				(*wl)[nick] = std::string(target->ident).append(" ").append(target->dhost).append(" ").append(ConvToStr(target->age));
				user->WriteServ("604 %s %s %s :is online",user->nick, nick, (*wl)[nick].c_str());
			}
			else
			{
				(*wl)[nick] = "";
				user->WriteServ("605 %s %s * * 0 :is offline",user->nick, nick);
			}
		}

		return CMD_SUCCESS;
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
							if (!q->second.empty())
								list.append(q->first.c_str()).append(" ");
						you_have = wl->size();
					}

					watchentries::iterator x = whos_watching_me.find(user->nick);
					if (x != whos_watching_me.end())
						youre_on = x->second.size();

					user->WriteServ("603 %s :You have %d and are on %d WATCH entries", user->nick, you_have, youre_on);
					if (!list.empty())
						user->WriteServ("606 %s :%s",user->nick, list.c_str());
					user->WriteServ("607 %s :End of WATCH S",user->nick);
				}
				else if (nick[0] == '-')
				{
					nick++;
					return remove_watch(user, nick);
				}
				else if (nick[0] == '+')
				{
					nick++;
					return add_watch(user, nick);
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
		: Module::Module(Me), maxwatch(32)
	{
		mycommand = new cmd_watch(ServerInstance, maxwatch);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserQuit] = List[I_OnPostConnect] = List[I_OnUserPostNick] = List[I_On005Numeric] = 1;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason)
	{
		ServerInstance->Log(DEBUG,"*** WATCH: On global quit: user %s",user->nick);
		watchentries::iterator x = whos_watching_me.find(user->nick);
		if (x != whos_watching_me.end())
		{
			for (std::deque<userrec*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
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
				watchentries::iterator x = whos_watching_me.find(i->first);
				if (x != whos_watching_me.end())
				{
						/* People are watching this user, am i one of them? */
						std::deque<userrec*>::iterator n = std::find(x->second.begin(), x->second.end(), user);
						if (n != x->second.end())
							/* I'm no longer watching you... */
							x->second.erase(n);
	
						if (!x->second.size())
							whos_watching_me.erase(user->nick);
				}
			}
		}
	}

	virtual void OnPostConnect(userrec* user)
	{
		ServerInstance->Log(DEBUG,"*** WATCH: On global connect: user %s",user->nick);
		watchentries::iterator x = whos_watching_me.find(user->nick);
		if (x != whos_watching_me.end())
		{
			for (std::deque<userrec*>::iterator n = x->second.begin(); n != x->second.end(); n++)
			{
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
		ServerInstance->Log(DEBUG,"*** WATCH: On global nickchange: old nick: %s new nick: %s",oldnick.c_str(),user->nick);

		watchentries::iterator new_online = whos_watching_me.find(user->nick);
		watchentries::iterator new_offline = whos_watching_me.find(assign(oldnick));

		if (new_online != whos_watching_me.end())
		{
			for (std::deque<userrec*>::iterator n = new_online->second.begin(); n != new_online->second.end(); n++)
			{
				watchlist* wl;
				if ((*n)->GetExt("watchlist", wl))
				{
					(*wl)[user->nick] = std::string(user->ident).append(" ").append(user->dhost).append(" ").append(ConvToStr(user->age));
					(*n)->WriteServ("600 %s %s %s %s %lu :arrived online", (*n)->nick, (*wl)[user->nick].c_str());
				}
			}
		}

		if (new_offline != whos_watching_me.end())
		{
			for (std::deque<userrec*>::iterator n = new_offline->second.begin(); n != new_offline->second.end(); n++)
			{
				watchlist* wl;
				if ((*n)->GetExt("watchlist", wl))
				{
 					(*n)->WriteServ("601 %s %s %s :went offline", (*n)->nick, (*wl)[user->nick].c_str());
					(*wl)[user->nick] = "";
				}
			}
		}
	}	

	virtual void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " WATCH=32";
	}
	
	virtual ~Modulewatch()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


class ModulewatchFactory : public ModuleFactory
{
 public:
	ModulewatchFactory()
	{
	}
	
	~ModulewatchFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new Modulewatch(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModulewatchFactory;
}

