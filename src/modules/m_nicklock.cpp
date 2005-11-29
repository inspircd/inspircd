/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit */

Server *Srv;

char* dummy = "ON";
	 
void handle_nicklock(char **parameters, int pcnt, userrec *user)
{
	userrec* source = Srv->FindNick(std::string(parameters[0]));
	if (source)
	{
		if (source->GetExt("nick_locked"))
		{
			WriteServ(user->fd,"946 %s %s :This user's nickname is already locked.",user->nick,source->nick);
			return;
		}
		if (Srv->IsNick(std::string(parameters[1])))
		{
			irc::string server = user->server;
			irc::string me = Srv->GetServerName().c_str();
			if (server == me)
			{
				// give them a lock flag
				Srv->SendOpers(std::string(user->nick)+" used NICKLOCK to change and hold "+std::string(parameters[0])+" to "+parameters[1]);
				Srv->ChangeUserNick(source,std::string(parameters[1]));
				// only attempt to set their lockflag after we know the change succeeded
				userrec* s2 = Srv->FindNick(std::string(parameters[1]));
				if (s2)
					s2->Extend("nick_locked",dummy);
			}
			else
			{
				WriteServ(user->fd,"947 %s %s :Can't lock the nickname of a non-local user",user->nick,source->nick);
			}
		}
	}
}

void handle_nickunlock(char **parameters, int pcnt, userrec *user)
{
	userrec* source = Srv->FindNick(std::string(parameters[0]));
	if (source)
	{
		source->Shrink("nick_locked");
		WriteServ(user->fd,"945 %s %s :Nickname now unlocked.",user->nick,source->nick);
		Srv->SendOpers(std::string(user->nick)+" used NICKUNLOCK on "+std::string(parameters[0]));
	}
}


class ModuleNickLock : public Module
{
 public:
	ModuleNickLock()
	{
		Srv = new Server;
		Srv->AddCommand("NICKLOCK",handle_nicklock,'o',2,"m_nicklock.so");
		Srv->AddCommand("NICKUNLOCK",handle_nickunlock,'o',1,"m_nicklock.so");
	}
	
	virtual ~ModuleNickLock()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

	virtual int OnUserPreNick(userrec* user, std::string newnick)
	{
		if (user->GetExt("nick_locked"))
		{
			WriteServ(user->fd,"447 %s :You cannot change your nickname (your nick is locked)",user->nick);
			return 1;
		}
		return 0;
	}

        virtual void OnUserQuit(userrec* user, std::string reason)
        {
                user->Shrink("nick_locked");
        }

};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleNickLockFactory : public ModuleFactory
{
 public:
	ModuleNickLockFactory()
	{
	}
	
	~ModuleNickLockFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleNickLock;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNickLockFactory;
}

