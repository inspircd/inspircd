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

#include "inspircd_config.h"
#include "users.h" 
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "wildcard.h"

void spy_userlist(userrec *user,chanrec *c)
{
	static char list[MAXBUF];

	if ((!c) || (!user))
		return;

	snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);

	CUList *ulist= c->GetUsers();
	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		strlcat(list,c->GetPrefixChar(i->second),MAXBUF);
		strlcat(list,i->second->nick,MAXBUF);
		strlcat(list," ",MAXBUF);
		if (strlen(list)>(480-NICKMAX))
		{
			/* list overflowed into
			 * multiple numerics */
			user->WriteServ(std::string(list));
			snprintf(list,MAXBUF,"353 %s = %s :", user->nick, c->name);
		}
	}
	/* if whats left in the list isnt empty, send it */
	if (list[strlen(list)-1] != ':')
	{
		user->WriteServ(std::string(list));
	}
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
		chanrec* c;

		if (!pcnt)
		{
			user->WriteServ("366 %s * :End of /NAMES list.",user->nick);
			return CMD_FAILURE;
		}

		if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 1))
			return CMD_FAILURE;

		ServerInstance->WriteOpers("*** Oper %s used SPYNAMES to view the users on %s",user->nick,parameters[0]);

		c = ServerInstance->FindChan(parameters[0]);
		if (c)
		{
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSpy(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSpyFactory;
}
