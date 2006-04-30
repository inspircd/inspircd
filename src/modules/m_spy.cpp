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

/* NO, THIS MODULE DOES NOT SPY ON CHANNELS OR USERS.
 * IT JUST ALLOWS OPERS TO SEE +s CHANNELS IN LIST AND
 * WHOIS, WHICH IS SUPPORTED BY MOST IRCDS IN CORE.
 */

/* $ModDesc: Provides SPYLIST and SPYNAMES capability, allowing opers to see who's in +s channels */

#include <stdio.h>
#include <vector>
#include <deque>
#include "globals.h"
#include "inspircd_config.h"
#include "users.h" 
#include "channels.h"
#include "modules.h"
#include "commands.h"
#include "socket.h"
#include "helperfuncs.h"
#include "inspircd.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "message.h"
#include "xline.h"
#include "typedefs.h"
#include "cull_list.h"
#include "aes.h"

extern InspIRCd* ServerInstance;
extern chan_hash chanlist;
extern time_t TIME;

typedef std::vector<userrec *> UserList;

UserList spy_listusers;    /* vector of people doing a /list */

void spy_userlist(userrec *user,chanrec *c)
{
	static char list[MAXBUF];

	if ((!c) || (!user))
		return;

	snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);

	CUList *ulist= c->GetUsers();
	
	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		strlcat(list,cmode(i->second,c),MAXBUF);
		strlcat(list,i->second->nick,MAXBUF);
		strlcat(list," ",MAXBUF);
	
		if (strlen(list)>(480-NICKMAX))
		{
			/* list overflowed into
			 * multiple numerics */
			WriteServ_NoFormat(user->fd,list);
			snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);
		}
	}
	
	/* if whats left in the list isnt empty, send it */
	if (list[strlen(list)-1] != ':')
	{
		WriteServ_NoFormat(user->fd,list);
	}
}

class SpyListData
{
 public:
	long list_start;
	long list_position;
	bool list_ended;

	SpyListData() : list_start(0), list_position(0), list_ended(false) {};
	SpyListData(long pos, time_t t) : list_start(t), list_position(pos), list_ended(false) {};
};

/*
 * To create a timer which recurs every second, we inherit from InspTimer.
 * InspTimer is only one-shot however, so at the end of each Tick() we simply
 * insert another of ourselves into the pending queue :)
 */
class SpyListTimer : public InspTimer
{
 private:

	Server* Srv;
	char buffer[MAXBUF];
	chanrec *chan;

 public:

	SpyListTimer(long interval, Server* Me) : InspTimer(interval,TIME), Srv(Me)
	{
	}

	virtual void Tick(time_t TIME)
	{
		bool go_again = true;

		while (go_again)
		{
			go_again = false;
			
			for (UserList::iterator iter = spy_listusers.begin(); iter != spy_listusers.end(); iter++)
			{
				/*
				 * What we do here:
				 *  - Get where they are up to
				 *  - If it's > GetChannelCount, erase them from the iterator, set go_again to true
				 *  - If not, spool the next 20 channels
				 */
				
				userrec* u = (userrec*)(*iter);
				SpyListData* ld = (SpyListData*)u->GetExt("safespylist_cache");

				if (ld->list_position > Srv->GetChannelCount())
				{
					u->Shrink("safespylist_cache");
					delete ld;
					spy_listusers.erase(iter);
					go_again = true;
					break;
				}

				log(DEBUG, "m_spy.so: resuming spool of list to client %s at channel %ld", u->nick, ld->list_position);
				chan = NULL;
				
				/* Attempt to fill up to half the user's sendq with /LIST output */
				long amount_sent = 0;
				
				do
				{
					log(DEBUG,"Channel %ld",ld->list_position);
					
					if (!ld->list_position)
						WriteServ(u->fd,"321 %s Channel :Users Name",u->nick);
					chan = Srv->GetChannelIndex(ld->list_position);
					
					/* spool details */
					if (chan)
					{
						long users = usercount(chan);

						if (users)
						{
							int counter = snprintf(buffer,MAXBUF,"322 %s %s %ld :[+%s] %s", u->nick, chan->name, users, chanmodes(chan, true), chan->topic);
							/* Increment total plus linefeed */
							amount_sent += counter + 4 + Srv->GetServerName().length();
							log(DEBUG,"m_safelist.so: Sent %ld of safe %ld / 4",amount_sent,u->sendqmax);
							WriteServ_NoFormat(u->fd,buffer);
						}
					}
					else
					{
						if (!chan)
						{
							if (!ld->list_ended)
							{
								ld->list_ended = true;
								WriteServ(u->fd,"323 %s :End of channel list.",u->nick);
							}
						}
					}

					ld->list_position++;
				}
				while ((chan != NULL) && (amount_sent < (u->sendqmax / 4)));
			}
		}

		SpyListTimer* MyTimer = new SpyListTimer(1,Srv);
		Srv->AddTimer(MyTimer);
	}
};

class cmd_spylist : public command_t
{
 public:
	cmd_spylist () : command_t("SPYLIST", 'o', 0)
	{
		this->source = "m_spy.so";
	}

	void Handle(char** parameters, int pcnt, userrec* user)
	{
		/* First, let's check if the user is currently /list'ing */
		SpyListData *ld = (SpyListData*)user->GetExt("safespylist_cache");
 
		if (ld)
		{
			/* user is already /spylist'ing, we don't want to do shit. */
			return;
		}

		WriteOpers("*** Oper %s used SPYLIST to list +s/+p channels and keys.", user->nick);
		
		time_t* last_list_time = (time_t*)user->GetExt("safespylist_last");
		if (last_list_time)
		{
			if (TIME < (*last_list_time)+60)
			{
				WriteServ(user->fd,"NOTICE %s :*** Woah there, slow down a little, you can't /LIST so often!",user->nick);
				return;
			}

			delete last_list_time;
			user->Shrink("safespylist_last");
		}
 
		/*
		 * start at channel 0! ;)
		 */
		ld = new SpyListData(0,TIME);
		user->Extend("safespylist_cache", (char*)ld);
		spy_listusers.push_back(user);

		time_t* llt = new time_t;
		*llt = TIME;
		user->Extend("safespylist_last",(char*)llt);
	
		return;
	}
};

class cmd_spynames : public command_t
{
 public:
	cmd_spynames() : command_t("SPYNAMES", 'o', 0)
	{
		this->source = "m_spy.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		chanrec* c;

		if (!pcnt)
		{
			WriteServ(user->fd,"366 %s * :End of /NAMES list.",user->nick);
			return;
		}

		if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-1,0))
			return;

		WriteOpers("*** Oper %s used SPYNAMES to view the users on %s",user->nick,parameters[0]);

		c = FindChan(parameters[0]);

		if (c)
		{
			spy_userlist(user,c);
			WriteServ(user->fd,"366 %s %s :End of /NAMES list.", user->nick, c->name);
		}
		else
		{
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
	}
};

class ModuleSpy : public Module
{
	Server* Srv;
	SpyListTimer* MyTimer;
	cmd_spylist *mycommand;
	cmd_spynames *mycommand2;
 public:
	ModuleSpy(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_spylist();
		mycommand2 = new cmd_spynames();
		MyTimer = new SpyListTimer(1, Srv);
		
		Srv->AddTimer(MyTimer);
		Srv->AddCommand(mycommand);
		Srv->AddCommand(mycommand2);
	}

	void Implements(char* List)
	{
		List[I_OnCleanup] = List[I_OnUserQuit] = 1;
	}
	
	virtual ~ModuleSpy()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* u = (userrec*)item;
			SpyListData* ld = (SpyListData*)u->GetExt("safespylist_cache");
			if (ld)
			{
				u->Shrink("safespylist_cache");
				delete ld;
			}
			
			for (UserList::iterator iter = spy_listusers.begin(); iter != spy_listusers.end(); iter++)
			{
				userrec* u2 = (userrec*)(*iter);
				if (u2 == u)
				{
					spy_listusers.erase(iter);
					break;
				}
			}
			
			time_t* last_list_time = (time_t*)u->GetExt("safespylist_last");
			
			if (last_list_time)
			{
				delete last_list_time;
				u->Shrink("safespylist_last");
			}
		}
	}

	virtual void OnUserQuit(userrec* user, const std::string &message)
	{
		this->OnCleanup(TYPE_USER,user);
	}
};
 
/******************************************************************************************************/
 
class ModuleSpyFactory : public ModuleFactory
{
 public:
	ModuleSpyFactory()
	{
	}
	
	~ModuleSpyFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSpy(Me);
	}
	
};
 
extern "C" void * init_module( void )
{
	return new ModuleSpyFactory;
}
