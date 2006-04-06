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

using namespace std;

/* $ModDesc: Provides SPYLIST and SPYNAMES capability, allowing opers to see who's in +s channels */

#include <stdio.h>
#include <vector>
#include <deque>
#include "globals.h"
#include "inspircd_config.h"
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
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

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif


static Server *Srv;

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern chan_hash chanlist;

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



class cmd_spylist : public command_t
{
  public:
	cmd_spylist () : command_t("SPYLIST", 'o', 0)
	{
		this->source = "m_spy.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		WriteOpers("*** Oper %s used SPYLIST to list +s/+p channels and keys.",user->nick);
		WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
		for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
		{
			WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,usercount(i->second),chanmodes(i->second,true),i->second->topic);
		}
		WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
	}
};

class cmd_spynames : public command_t
{
  public:
	  cmd_spynames () : command_t("SPYNAMES", 'o', 0)
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
	cmd_spylist *mycommand;
	cmd_spynames *mycommand2;
 public:
	ModuleSpy(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_spylist();
		mycommand2 = new cmd_spynames();
		Srv->AddCommand(mycommand);
		Srv->AddCommand(mycommand2);
	}
	
	virtual ~ModuleSpy()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}
};


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
