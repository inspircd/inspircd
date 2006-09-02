#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "mode.h"

#include "u_listmode.h"

/* $ModDesc: Provides support for the +I channel mode */

/*
 * Written by Om <om@inspircd.org>, April 2005.
 * Based on m_exception, which was originally based on m_chanprotect and m_silence
 *
 * The +I channel mode takes a nick!ident@host, glob patterns allowed,
 * and if a user matches an entry on the +I list then they can join the channel,
 * ignoring if +i is set on the channel
 */

class InspIRCd* ServerInstance;

class InviteException : public ListModeBase
{
 public:
	InviteException(InspIRCd* Instance) : ListModeBase(Instance, 'I', "End of Channel Invite Exception List", "346", "347", true) { }
};

class ModuleInviteException : public Module
{
	InviteException* ie;
	

public:
	ModuleInviteException(InspIRCd* Me) : Module(Me)
	{
		ie = new InviteException(ServerInstance);
		ServerInstance->AddMode(ie, 'I');
	}
	
	virtual void Implements(char* List)
	{
		ie->DoImplements(List);
		List[I_On005Numeric] = List[I_OnCheckInvite] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		output.append(" INVEX=I");
	}
	 
	virtual int OnCheckInvite(userrec* user, chanrec* chan)
	{
		if(chan != NULL)
		{
			modelist* list;
			chan->GetExt(ie->GetInfoKey(), list);
			if (list)
			{
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					if(match(user->GetFullRealHost(), it->mask.c_str()) || match(user->GetFullHost(), it->mask.c_str()))
					{
						// They match an entry on the list, so let them in.
						return 1;
					}
				}
			}
			// or if there wasn't a list, there can't be anyone on it, so we don't need to do anything.
		}

		return 0;		
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		ie->DoCleanup(target_type, item);
	}

	virtual void OnSyncChannel(chanrec* chan, Module* proto, void* opaque)
	{
		ie->DoSyncChannel(chan, proto, opaque);
	}

	virtual void OnChannelDelete(chanrec* chan)
	{
		ie->DoChannelDelete(chan);
	}

	virtual void OnRehash(const std::string &param)
	{
		ie->DoRehash();
	}
		
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 3, VF_VENDOR | VF_COMMON);
	}

	~ModuleInviteException()
	{
		ServerInstance->Modes->DelMode(ie);
		DELETE(ie);
	}
};


class ModuleInviteExceptionFactory : public ModuleFactory
{
 public:
	ModuleInviteExceptionFactory()
	{
	}
	
	~ModuleInviteExceptionFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleInviteException(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleInviteExceptionFactory;
}
