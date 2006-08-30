/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *   E-mail:
 *	<brain@chatspike.net>
 *	<Craig@chatspike.net>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

static const char* dummy = "ON";

/* $ModDesc: Provides aliases of commands. */

class ModuleNamesX : public Module
{
 public:
	
	ModuleNamesX(InspIRCd* Me)
		: Module::Module(Me)
	{
	}

	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_OnUserList] = List[I_On005Numeric] = 1;
	}

	virtual ~ModuleNamesX()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

        virtual void On005Numeric(std::string &output)
	{
		output.append(" NAMESX");
	}

        virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated)
	{
		irc::string c = command.c_str();
		/* We don't actually create a proper command handler class for PROTOCTL,
		 * because other modules might want to have PROTOCTL hooks too.
		 * Therefore, we just hook its as an unvalidated command therefore we
		 * can capture it even if it doesnt exist! :-)
		 */
		if (c == "PROTOCTL")
		{
			if ((pcnt) && (!strcasecmp(parameters[0],"NAMESX")))
			{
				ServerInstance->Log(DEBUG,"Setting this user as NAMESX capable");
				user->Extend("NAMESX",dummy);
				return 1;
			}
		}
		return 0;
	}

	virtual int OnUserList(userrec* user, chanrec* Ptr)
	{
		ServerInstance->Log(DEBUG,"NAMESX called for %s %s",user->nick,Ptr->name);
		if (user->GetExt("NAMESX"))
		{
			ServerInstance->Log(DEBUG,"Using NAMESX user list code");
			char list[MAXBUF];
			size_t dlen, curlen;
			dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, Ptr->name);
			int numusers = 0;
			char* ptr = list + dlen;
			CUList *ulist= Ptr->GetUsers();
			bool has_user = Ptr->HasUser(user);
			for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
			{
				if ((!has_user) && (i->second->modes[UM_INVISIBLE]))
				{
					continue;
				}
				size_t ptrlen = snprintf(ptr, MAXBUF, "%s%s ", Ptr->GetAllPrefixChars(i->second), i->second->nick);
				curlen += ptrlen;
				ptr += ptrlen;
				numusers++;
				if (curlen > (480-NICKMAX))
				{
					/* list overflowed into multiple numerics */
					ServerInstance->Log(DEBUG,"Send list 1");
					user->WriteServ(list);
					/* reset our lengths */
					dlen = curlen = snprintf(list,MAXBUF,"353 %s = %s :", user->nick, Ptr->name);
					ptr = list + dlen;
					ptrlen = 0;
					numusers = 0;
				}
			}
			/* if whats left in the list isnt empty, send it */
			if (numusers)
			{
				ServerInstance->Log(DEBUG,"Send list 2");
				user->WriteServ(list);
			}
			user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, Ptr->name);
			ServerInstance->Log(DEBUG,"Returning 1");
			return 1;
		}

		ServerInstance->Log(DEBUG,"Returning 0");
		return 0;		
 	}
};


class ModuleNamesXFactory : public ModuleFactory
{
 public:
	ModuleNamesXFactory()
	{
	}

	~ModuleNamesXFactory()
	{
	}

		virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleNamesX(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleNamesXFactory;
}

