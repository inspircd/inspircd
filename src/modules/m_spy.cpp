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

/** Handle /SPYNAMES
 */
class CommandSpynames : public Command
{
  public:
	CommandSpynames (InspIRCd* Instance) : Command(Instance,"SPYNAMES", 'o', 0)
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
	CommandSpynames *mycommand2;
 public:
	ModuleSpy(InspIRCd* Me) : Module(Me)
	{
		
		mycommand2 = new CommandSpynames(ServerInstance);
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

