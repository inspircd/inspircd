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

/* NO, THIS MODULE DOES NOT SPY ON CHANNELS OR USERS.
 * IT JUST ALLOWS OPERS TO SEE +s CHANNELS IN LIST AND
 * WHOIS, WHICH IS SUPPORTED BY MOST IRCDS IN CORE.
 */

using namespace std;

/* $ModDesc: Provides SPYLIST and SPYNAMES capability, allowing opers to see who's in +s channels */

#include "inspircd_config.h"
#include "users.h" 
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "wildcard.h"

void spy_userlist(userrec *user, chanrec *c)
{
	char list[MAXBUF];
	size_t dlen, curlen;

	dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);

	int numusers = 0;
	char* ptr = list + dlen;

	CUList *ulist= c->GetUsers();

	/* Improvement by Brain - this doesnt change in value, so why was it inside
	 * the loop?
	 */
	bool has_user = c->HasUser(user);

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((!has_user) && (i->second->modes[UM_INVISIBLE]))
		{
			/*
			 * user is +i, and source not on the channel, does not show
			 * nick in NAMES list
			 */
			continue;
		}

		size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", c->GetPrefixChar(i->second), i->second->nick);

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			/* list overflowed into multiple numerics */
			user->WriteServ(std::string(list));

			/* reset our lengths */
			dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);
			ptr = list + dlen;

			ptrlen = 0;
			numusers = 0;
		}
	}

	/* if whats left in the list isnt empty, send it */
	if (numusers)
	{
		user->WriteServ(std::string(list));
	}

	user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, c->name);

}


class cmd_spylist : public command_t
{
  public:
	cmd_spylist (InspIRCd* Instance) : command_t(Instance,"SPYLIST", 'o', 0)
	{
		this->source = "m_spy.so";
		syntax = "";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		ServerInstance->WriteOpers("*** Oper %s used SPYLIST to list +s/+p channels and keys.",user->nick);
		user->WriteServ("321 %s Channel :Users Name",user->nick);
		for (chan_hash::const_iterator i = ServerInstance->chanlist.begin(); i != ServerInstance->chanlist.end(); i++)
		{
			if (pcnt && !match(i->second->name, parameters[0]))
				continue;
			user->WriteServ("322 %s %s %d :[+%s] %s",user->nick,i->second->name,i->second->GetUserCounter(),i->second->ChanModes(true),i->second->topic);
		}
		user->WriteServ("323 %s :End of channel list.",user->nick);

		/* Dont send out across the network */
		return CMD_FAILURE;
	}
};

class cmd_spynames : public command_t
{
  public:
	cmd_spynames (InspIRCd* Instance) : command_t(Instance,"SPYNAMES", 'o', 0)
	{
		this->source = "m_spy.so";
		syntax = "{<channel>{,<channel>}}";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		chanrec* c = NULL;

		if (!pcnt)
		{
			user->WriteServ("366 %s * :End of /NAMES list.",user->nick);
			return CMD_FAILURE;
		}

		if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
			return CMD_FAILURE;

		c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
			ServerInstance->WriteOpers("*** Oper %s used SPYNAMES to view the users on %s", user->nick, parameters[0]);
			spy_userlist(user,c);
			user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, c->name);
		}
		else
		{
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}

		return CMD_FAILURE;
	}
};

class ModuleSpy : public Module
{
	cmd_spylist *mycommand;
	cmd_spynames *mycommand2;
 public:
	ModuleSpy(InspIRCd* Me) : Module::Module(Me)
	{
		
		mycommand = new cmd_spylist(ServerInstance);
		mycommand2 = new cmd_spynames(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		ServerInstance->AddCommand(mycommand2);
	}
	
	virtual ~ModuleSpy()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSpy(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSpyFactory;
}
