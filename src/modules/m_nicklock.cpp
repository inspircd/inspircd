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
#include "inspircd.h"

/* $ModDesc: Provides the NICKLOCK command, allows an oper to chage a users nick and lock them to it until they quit */

extern InspIRCd* ServerInstance;

class cmd_nicklock : public command_t
{
	char* dummy;
 public:
 cmd_nicklock (InspIRCd* Instance) : command_t(Instance,"NICKLOCK", 'o', 2)
	{
		this->source = "m_nicklock.so";
		syntax = "<oldnick> <newnick>";
	}

	void Handle(const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = ServerInstance->FindNick(parameters[0]);
		irc::string server;
		irc::string me;

		if (source)
		{
			if (source->GetExt("nick_locked", dummy))
			{
				user->WriteServ("946 %s %s :This user's nickname is already locked.",user->nick,source->nick);
				return;
			}
			if (ServerInstance->IsNick(parameters[1]))
			{
				// give them a lock flag
				ServerInstance->WriteOpers(std::string(user->nick)+" used NICKLOCK to change and hold "+parameters[0]+" to "+parameters[1]);
				if (!source->ForceNickChange(parameters[1]))
				{
					userrec::QuitUser(ServerInstance, source, "Nickname collision");
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
 cmd_nickunlock (InspIRCd* Instance) : command_t(Instance,"NICKUNLOCK", 'o', 1)
	{
		this->source = "m_nickunlock.so";
		syntax = "<locked-nick>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = ServerInstance->FindNick(parameters[0]);
		if (source)
		{
			source->Shrink("nick_locked");
			user->WriteServ("945 %s %s :Nickname now unlocked.",user->nick,source->nick);
			ServerInstance->WriteOpers(std::string(user->nick)+" used NICKUNLOCK on "+parameters[0]);
		}
	}
};


class ModuleNickLock : public Module
{
	cmd_nicklock*	cmd1;
	cmd_nickunlock*	cmd2;
	char* n;
 public:
	ModuleNickLock(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		cmd1 = new cmd_nicklock(ServerInstance);
		cmd2 = new cmd_nickunlock(ServerInstance);
		ServerInstance->AddCommand(cmd1);
		ServerInstance->AddCommand(cmd2);
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
			user->WriteServ("447 %s :You cannot change your nickname (your nick is locked)",user->nick);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleNickLock(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNickLockFactory;
}

