#include <stdio.h>
#include <string>
#include <vector>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "u_listmode.h"

/* $ModDesc: Provides support for the +e channel mode */

/* Written by Om<om@inspircd.org>, April 2005. */
/* Rewritten to use the listmode utility by Om, December 2005 */
/* Adapted from m_exception, which was originally based on m_chanprotect and m_silence */

// The +e channel mode takes a nick!ident@host, glob patterns allowed,
// and if a user matches an entry on the +e list then they can join the channel, overriding any (+b) bans set on them


class ModuleBanException : public ListModeBaseModule
{
public:
	ModuleBanException(Server* serv) : ListModeBaseModule::ListModeBaseModule(serv, 'e', "End of Channel Exception List", "348", "349")
	{
	}
	
	virtual void Implements(char* List)
	{
		this->DoImplements(List);
		List[I_On005Numeric] = List[I_OnCheckBan] = 1;
	}
	
	virtual void On005Numeric(std::string &output)
	{
		output.append(" EXCEPTS");
		output.insert(output.find("CHANMODES=", 0)+10, "e");
	}

	virtual int OnCheckBan(userrec* user, chanrec* chan)
	{
		if(chan != NULL)
		{
			modelist* list = (modelist*)chan->GetExt(infokey);
			Srv->Log(DEBUG, std::string(user->nick)+" is trying to join "+std::string(chan->name)+", checking for ban exceptions");
			
			if(list)
			{
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
					if(Srv->MatchText(user->GetFullRealHost(), it->mask) || Srv->MatchText(user->GetFullHost(), it->mask))
						// They match an entry on the list, so let them in.
						return 1;
				return 0;
			}
			// or if there wasn't a list, there can't be anyone on it, so we don't need to do anything.
		}
		return 0;	
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 2, VF_STATIC);
	}
};

class ModuleBanExceptionFactory : public ModuleFactory
{
 public:
	ModuleBanExceptionFactory()
	{
	}
	
	~ModuleBanExceptionFactory()
	{
	}
	
	virtual Module* CreateModule(Server* serv)
	{
		return new ModuleBanException(serv);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBanExceptionFactory;
}
