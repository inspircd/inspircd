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
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "inspircd.h"

/* $ModDesc: Forbids users from messaging each other. Users may still message opers and opers may message other opers. */


class ModuleRestrictMsg : public Module
{
	
 public:
 
	ModuleRestrictMsg(InspIRCd* Me)
		: Module::Module(Me)
	{
		
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_USER)
		{
			userrec* u = (userrec*)dest;
			if (*u->oper || *user->oper)
			{
				// message allowed if:
				// (1) the sender is opered
				// (2) the recipient is opered
				// (3) both are opered
				// anything else, blocked.
				return 0;
			}
			user->WriteServ("531 %s %s :You are not permitted to send private messages to this user",user->nick,u->nick);
			return 1;
		}
		// however, we must allow channel messages...
		return 0;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return this->OnUserPreMessage(user,dest,target_type,text,status);
	}

	virtual ~ModuleRestrictMsg()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
};


class ModuleRestrictMsgFactory : public ModuleFactory
{
 public:
	ModuleRestrictMsgFactory()
	{
	}
	
	~ModuleRestrictMsgFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleRestrictMsg(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleRestrictMsgFactory;
}

