/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
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




class watchentry : public classbase
{
 public:
	userrec* watcher;
	std::string target;
};

typedef std::vector<watchentry*> watchlist;
watchlist watches;

class cmd_watch : public command_t
{
 public:
 cmd_watch (InspIRCd* Instance) : command_t(Instance,"WATCH",0,0)
	{
		this->source = "m_watch.so";
		syntax = "[C|L|S]|[+|-<nick>]";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (!pcnt)
		{
			for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
			{
				watchentry* a = (watchentry*)(*q);
				if (a->watcher == user)
				{
					userrec* targ = ServerInstance->FindNick(a->target);
					if (targ)
					{
						user->WriteServ("604 %s %s %s %s %lu :is online",user->nick,targ->nick,targ->ident,targ->dhost,targ->age);
					}
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
					bool done = false;
					while (!done)
					{
						done = true;
						for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
						{
							watchentry* a = (watchentry*)(*q);
							if (a->watcher == user)
							{
								done = false;
								watches.erase(q);
								delete a;
								break;
							}
						}
					}
				}
				else if (!strcasecmp(nick,"L"))
				{
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						watchentry* a = (watchentry*)(*q);
						if (a->watcher == user)
						{
							userrec* targ = ServerInstance->FindNick(a->target);
							if (targ)
							{
								user->WriteServ("604 %s %s %s %s %lu :is online",user->nick,targ->nick,targ->ident,targ->dhost,targ->age);
							}
						}
					}
					user->WriteServ("607 %s :End of WATCH list",user->nick);
				}
				else if (!strcasecmp(nick,"S"))
				{
					std::string list = "";
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						watchentry* a = (watchentry*)(*q);
						if (a->watcher == user)
						{
							list.append(" ").append(a->target);
						}
					}
					char* l = (char*)list.c_str();
					if (*l == ' ')
						l++;
					user->WriteServ("606 %s :%s",user->nick,l);
					user->WriteServ("607 %s :End of WATCH S",user->nick);
				}
				else if (nick[0] == '-')
				{
					// removing an item from the list
					nick++;
					if (!ServerInstance->IsNick(nick))
					{
						user->WriteServ("942 %s %s :Invalid nickname",user->nick,nick);
						return;
					}
					irc::string n1 = nick;
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						watchentry* b = (watchentry*)(*q);
						if (b->watcher == user)
						{
							irc::string n2 = b->target.c_str();
							userrec* a = ServerInstance->FindNick(b->target);
							if (a)
							{
								user->WriteServ("602 %s %s %s %s %lu :stopped watching",user->nick,a->nick,a->ident,a->dhost,a->age);
							}
							else
							{
								user->WriteServ("602 %s %s * * 0 :stopped watching",user->nick,b->target.c_str());
							}
							if (n1 == n2)
							{
								watches.erase(q);
								delete b;
								break;
							}
						}
					}
				}
				else if (nick[0] == '+')
				{
					nick++;
					if (!ServerInstance->IsNick(nick))
					{
						user->WriteServ("942 %s %s :Invalid nickname",user->nick,nick);
						return;
					}
					irc::string n1 = nick;
					bool exists = false;
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						watchentry* a = (watchentry*)(*q);
						if (a->watcher == user)
						{
							irc::string n2 = a->target.c_str();
							if (n1 == n2)
							{
								// already on watch list
								exists = true;
							}
						}
					}
					if (!exists)
					{
						watchentry* w = new watchentry();
						w->watcher = user;
						w->target = nick;
						watches.push_back(w);
						ServerInstance->Log(DEBUG,"*** Added %s to watchlist of %s",nick,user->nick);
					}
		       			userrec* a = ServerInstance->FindNick(nick);
		       			if (a)
		       			{
		       				user->WriteServ("604 %s %s %s %s %lu :is online",user->nick,a->nick,a->ident,a->dhost,a->age);
					}
					else
					{
						user->WriteServ("605 %s %s * * 0 :is offline",user->nick,nick);
					}
				}
			}
		}
		return;
	}
};

class Modulewatch : public Module
{
	cmd_watch* mycommand;
 public:

	Modulewatch(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_watch(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserQuit] = List[I_OnPostConnect] = List[I_OnUserPostNick] = List[I_On005Numeric] = 1;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason)
	{
		ServerInstance->Log(DEBUG,"*** WATCH: On global quit: user %s",user->nick);
		irc::string n2 = user->nick;
		for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
		{
			watchentry* a = (watchentry*)(*q);
			irc::string n1 = a->target.c_str();
			if (n1 == n2)
			{
				ServerInstance->Log(DEBUG,"*** WATCH: On global quit: user %s is in notify of %s",user->nick,a->watcher->nick);
				a->watcher->WriteServ("601 %s %s %s %s %lu :went offline",a->watcher->nick,user->nick,user->ident,user->dhost,time(NULL));
			}
		}
		bool done = false;
		while (!done)
		{
			done = true;
			for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
			{
				watchentry* a = (watchentry*)(*q);
				if (a->watcher == user)
				{
					done = false;
					watches.erase(q);
					delete a;
					break;
				}
			}
		}
	}

	virtual void OnPostConnect(userrec* user)
	{
		irc::string n2 = user->nick;
		ServerInstance->Log(DEBUG,"*** WATCH: On global connect: user %s",user->nick);
		for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
		{
			watchentry* a = (watchentry*)(*q);
			irc::string n1 = a->target.c_str();
			if (n1 == n2)
			{
				ServerInstance->Log(DEBUG,"*** WATCH: On global connect: user %s is in notify of %s",user->nick,a->watcher->nick);
				a->watcher->WriteServ("600 %s %s %s %s %lu :arrived online",a->watcher->nick,user->nick,user->ident,user->dhost,user->age);
			}
		}
	}

	virtual void OnUserPostNick(userrec* user, const std::string &oldnick)
	{
		irc::string n2 = oldnick.c_str();
		irc::string n3 = user->nick;
		ServerInstance->Log(DEBUG,"*** WATCH: On global nickchange: old nick: %s new nick: %s",oldnick.c_str(),user->nick);
		for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
		{
			watchentry* a = (watchentry*)(*q);
			irc::string n1 = a->target.c_str();
			// changed from a nick on the watchlist to one that isnt
			if (n1 == n2)
			{
				ServerInstance->Log(DEBUG,"*** WATCH: On global nickchange: old nick %s was on notify list of %s",oldnick.c_str(),a->watcher->nick);
				a->watcher->WriteServ("601 %s %s %s %s %lu :went offline",a->watcher->nick,oldnick.c_str(),user->ident,user->dhost,time(NULL));
			}
			else if (n1 == n3)
			{
				// changed from a nick not on notify to one that is
				ServerInstance->Log(DEBUG,"*** WATCH: On global nickchange: new nick %s is on notify list of %s",user->nick,a->watcher->nick);
				a->watcher->WriteServ("600 %s %s %s %s %lu :arrived online",a->watcher->nick,user->nick,user->ident,user->dhost,user->age);
			}
		}
	}	

	virtual void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " WATCH=999";
	}
	
	virtual ~Modulewatch()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
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

