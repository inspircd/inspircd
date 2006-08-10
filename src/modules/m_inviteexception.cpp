#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "mode.h"
#include "helperfuncs.h"
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
	InviteException(Server* serv) : ListModeBase(serv, 'I', "End of Channel Invite Exception List", "346", "347", true) { }
};

class ModuleInviteException : public Module
{
	InviteException* ie;
	Server* Srv;

public:
	ModuleInviteException(Server* serv) : Module(serv)
	{
		ie = new InviteException(serv);
		Srv = serv;
		Srv->AddMode(ie, 'I');
	}
	
	virtual void Implements(char* List)
	{
		ie->DoImplements(List);
		List[I_On005Numeric] = List[I_OnCheckInvite] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		output.append(" INVEX=I");
		ServerInstance->ModeGrok->InsertMode(output, "I", 1);
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
					if(Srv->MatchText(user->GetFullRealHost(), it->mask) || Srv->MatchText(user->GetFullHost(), it->mask))
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
		return Version(1, 0, 0, 3, VF_VENDOR | VF_STATIC);
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
	
	virtual Module * CreateModule(Server* serv)
	{
		return new ModuleInviteException(serv);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleInviteExceptionFactory;
}
