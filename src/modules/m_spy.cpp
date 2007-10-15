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

/* $ModDesc: Provides SPYLIST and SPYNAMES capability, allowing opers to see who's in +s channels */

#include "inspircd.h"
#include "wildcard.h"

void spy_userlist(User *user, Channel *c)
{
	char list[MAXBUF];
	size_t dlen, curlen;

	dlen = curlen = snprintf(list,MAXBUF,"353 %s %c %s :", user->nick, c->IsModeSet('s') ? '@' : c->IsModeSet('p') ? '*' : '=', c->name);

	int numusers = 0;
	char* ptr = list + dlen;

	CUList *ulist= c->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", c->GetPrefixChar(i->first), i->first->nick);

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			/* list overflowed into multiple numerics */
			user->WriteServ(std::string(list));

			/* reset our lengths */
			dlen = curlen = snprintf(list,MAXBUF,"353 %s %c %s :", user->nick, c->IsModeSet('s') ? '@' : c->IsModeSet('p') ? '*' : '=', c->name);
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

/** Handle /SPYLIST
 */
class cmd_spylist : public Command
{
  public:
	cmd_spylist (InspIRCd* Instance) : Command(Instance,"SPYLIST", 'o', 0)
	{
		this->source = "m_spy.so";
		syntax.clear();
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		ServerInstance->WriteOpers("*** Oper %s used SPYLIST to list +s/+p channels and keys.",user->nick);
		user->WriteServ("321 %s Channel :Users Name",user->nick);
		for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); i++)
		{
			if (pcnt && !match(i->second->name, parameters[0]))
				continue;
			user->WriteServ("322 %s %s %d :[+%s] %s",user->nick,i->second->name,i->second->GetUserCounter(),i->second->ChanModes(true),i->second->topic);
		}
		user->WriteServ("323 %s :End of channel list.",user->nick);

		/* Dont send out across the network */
		return CMD_LOCALONLY;
	}
};

/** Handle /SPYNAMES
 */
class cmd_spynames : public Command
{
  public:
	cmd_spynames (InspIRCd* Instance) : Command(Instance,"SPYNAMES", 'o', 0)
	{
		this->source = "m_spy.so";
		syntax = "{<channel>{,<channel>}}";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		Channel* c = NULL;

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
		}
		else
		{
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}

		return CMD_LOCALONLY;
	}
};

class ModuleSpy : public Module
{
	cmd_spylist *mycommand;
	cmd_spynames *mycommand2;
 public:
	ModuleSpy(InspIRCd* Me) : Module(Me)
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
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSpy)
