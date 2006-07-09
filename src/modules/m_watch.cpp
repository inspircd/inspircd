/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
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
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Provides support for the /watch command */

static Server *Srv;

class watchentry
{
 public:
	userrec* watcher;
	std::string target;
};

typedef std::vector<watchentry> watchlist;
watchlist watches;

class cmd_watch : public command_t
{
 public:
	cmd_watch() : command_t("WATCH",0,0)
	{
		this->source = "m_watch.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		if (!pcnt)
		{
			for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
			{
				if (q->watcher == user)
				{
					userrec* targ = Srv->FindNick(q->target);
					if (targ)
					{
						WriteServ(user->fd,"604 %s %s %s %s %lu :is online",user->nick,targ->nick,targ->ident,targ->dhost,targ->age);
					}
				}
			}
			WriteServ(user->fd,"607 %s :End of WATCH list",user->nick);
		}
		else if (pcnt > 0)
		{
			for (int x = 0; x < pcnt; x++)
			{
				char *nick = parameters[x];
				if (!strcasecmp(nick,"C"))
				{
					// watch clear
					bool done = false;
					while (!done)
					{
						done = true;
						for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
						{
							if (q->watcher == user)
							{
								done = false;
								watches.erase(q);
								break;
							}
						}
					}
				}
				else if (!strcasecmp(nick,"L"))
				{
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						if (q->watcher == user)
						{
							userrec* targ = Srv->FindNick(q->target);
							if (targ)
							{
								WriteServ(user->fd,"604 %s %s %s %s %lu :is online",user->nick,targ->nick,targ->ident,targ->dhost,targ->age);
							}
						}
					}
					WriteServ(user->fd,"607 %s :End of WATCH list",user->nick);
				}
				else if (!strcasecmp(nick,"S"))
				{
					std::string list = "";
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						if (q->watcher == user)
						{
							list = list + " " + q->target;
						}
					}
					char* l = (char*)list.c_str();
					if (*l == ' ')
						l++;
					WriteServ(user->fd,"606 %s :%s",user->nick,l);
					WriteServ(user->fd,"607 %s :End of WATCH S",user->nick);
				}
				else if (nick[0] == '-')
				{
					// removing an item from the list
					nick++;
					irc::string n1 = nick;
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						if (q->watcher == user)
						{
							irc::string n2 = q->target.c_str();
							userrec* a = Srv->FindNick(q->target);
							if (a)
							{
								WriteServ(user->fd,"602 %s %s %s %s %lu :stopped watching",user->nick,a->nick,a->ident,a->dhost,a->age);
							}
							else
							{
								 WriteServ(user->fd,"602 %s %s * * 0 :stopped watching",user->nick,q->target.c_str());
							}
							if (n1 == n2)
							{
								watches.erase(q);
								break;
							}
						}
					}
				}
				else if (nick[0] == '+')
				{
					nick++;
					irc::string n1 = nick;
					bool exists = false;
					for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
					{
						if (q->watcher == user)
						{
							irc::string n2 = q->target.c_str();
							if (n1 == n2)
							{
								// already on watch list
								exists = true;
							}
						}
					}
					if (!exists)
					{
						watchentry w;
						w.watcher = user;
						w.target = nick;
						watches.push_back(w);
						log(DEBUG,"*** Added %s to watchlist of %s",nick,user->nick);
					}
		       			userrec* a = Srv->FindNick(nick);
		       			if (a)
		       			{
		       				WriteServ(user->fd,"604 %s %s %s %s %lu :is online",user->nick,a->nick,a->ident,a->dhost,a->age);
					}
					else
					{
						WriteServ(user->fd,"605 %s %s * * 0 :is offline",user->nick,nick);
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

	Modulewatch(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_watch();
		Srv->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserQuit] = List[I_OnGlobalConnect] = List[I_OnUserPostNick] = List[I_On005Numeric] = 1;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason)
	{
		log(DEBUG,"*** WATCH: On global quit: user %s",user->nick);
		irc::string n2 = user->nick;
		for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
		{
			irc::string n1 = q->target.c_str();
			if (n1 == n2)
			{
				log(DEBUG,"*** WATCH: On global quit: user %s is in notify of %s",user->nick,q->watcher->nick);
				WriteServ(q->watcher->fd,"601 %s %s %s %s %lu :went offline",q->watcher->nick,user->nick,user->ident,user->dhost,time(NULL));
			}
		}
		bool done = false;
		while (!done)
		{
			done = true;
			for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
			{
				if (q->watcher == user)
				{
					done = false;
					watches.erase(q);
					break;
				}
			}
		}
	}

	virtual void OnGlobalConnect(userrec* user)
	{
		irc::string n2 = user->nick;
		log(DEBUG,"*** WATCH: On global connect: user %s",user->nick);
		for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
		{
			irc::string n1 = q->target.c_str();
			if (n1 == n2)
			{
				log(DEBUG,"*** WATCH: On global connect: user %s is in notify of %s",user->nick,q->watcher->nick);
				WriteServ(q->watcher->fd,"600 %s %s %s %s %lu :arrived online",q->watcher->nick,user->nick,user->ident,user->dhost,user->age);
			}
		}
	}

	virtual void OnUserPostNick(userrec* user, const std::string &oldnick)
	{
		irc::string n2 = oldnick.c_str();
		irc::string n3 = user->nick;
		log(DEBUG,"*** WATCH: On global nickchange: old nick: %s new nick: %s",oldnick.c_str(),user->nick);
		for (watchlist::iterator q = watches.begin(); q != watches.end(); q++)
		{
			irc::string n1 = q->target.c_str();
			// changed from a nick on the watchlist to one that isnt
			if (n1 == n2)
			{
				log(DEBUG,"*** WATCH: On global nickchange: old nick %s was on notify list of %s",oldnick.c_str(),q->watcher->nick);
				WriteServ(q->watcher->fd,"601 %s %s %s %s %lu :went offline",q->watcher->nick,oldnick.c_str(),user->ident,user->dhost,time(NULL));
			}
			else if (n1 == n3)
			{
				// changed from a nick not on notify to one that is
				log(DEBUG,"*** WATCH: On global nickchange: new nick %s is on notify list of %s",user->nick,q->watcher->nick);
				WriteServ(q->watcher->fd,"600 %s %s %s %s %lu :arrived online",q->watcher->nick,user->nick,user->ident,user->dhost,user->age);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new Modulewatch(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModulewatchFactory;
}

