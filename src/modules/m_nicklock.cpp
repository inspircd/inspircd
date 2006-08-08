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
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit */

static Server *Srv;

class cmd_nicklock : public command_t
{
	char* dummy;
 public:
	cmd_nicklock () : command_t("NICKLOCK", 'o', 2)
	{
		this->source = "m_nicklock.so";
		syntax = "<oldnick> <newnick>";
	}

	void Handle(const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = Srv->FindNick(std::string(parameters[0]));
		irc::string server;
		irc::string me;

		if (source)
		{
			if (source->GetExt("nick_locked", dummy))
			{
				WriteServ(user->fd,"946 %s %s :This user's nickname is already locked.",user->nick,source->nick);
				return;
			}
			if (Srv->IsNick(std::string(parameters[1])))
			{
				// give them a lock flag
				Srv->SendOpers(std::string(user->nick)+" used NICKLOCK to change and hold "+std::string(parameters[0])+" to "+parameters[1]);
				if (!source->ForceNickChange(parameters[1]))
				{
					userrec::QuitUser(source, "Nickname collision");
					return;
				}
				source->Extend("nick_locked", "ON");
			}
		}
	}
};

class cmd_nickunlock : public command_t
{
 public:
	cmd_nickunlock () : command_t("NICKUNLOCK", 'o', 1)
	{
		this->source = "m_nickunlock.so";
		syntax = "<locked-nick>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = Srv->FindNick(std::string(parameters[0]));
		if (source)
		{
			source->Shrink("nick_locked");
			WriteServ(user->fd,"945 %s %s :Nickname now unlocked.",user->nick,source->nick);
			Srv->SendOpers(std::string(user->nick)+" used NICKUNLOCK on "+std::string(parameters[0]));
		}
	}
};


class ModuleNickLock : public Module
{
	cmd_nicklock*	cmd1;
	cmd_nickunlock*	cmd2;
	char* n;
 public:
	ModuleNickLock(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		cmd1 = new cmd_nicklock();
		cmd2 = new cmd_nickunlock();
		Srv->AddCommand(cmd1);
		Srv->AddCommand(cmd2);
	}
	
	virtual ~ModuleNickLock()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNick] = List[I_OnUserQuit] = List[I_OnCleanup] = 1;
	}

	virtual int OnUserPreNick(userrec* user, const std::string &newnick)
	{
		if (user->GetExt("nick_locked", n))
		{
			WriteServ(user->fd,"447 %s :You cannot change your nickname (your nick is locked)",user->nick);
			return 1;
		}
		return 0;
	}

	virtual void OnUserQuit(userrec* user, const std::string &reason)
	{
		user->Shrink("nick_locked");
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			userrec* user = (userrec*)item;
			user->Shrink("nick_locked");
		}
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleNickLock(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNickLockFactory;
}

