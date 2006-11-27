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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for ircu style usermode +d (deaf to channel messages and channel notices) */

/** User mode +d - filter out channel messages and channel notices
 */
class User_d : public ModeHandler
{
 public:
	User_d(InspIRCd* Instance) : ModeHandler(Instance, 'd', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('d'))
			{
				dest->SetMode('d',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('d'))
			{
				dest->SetMode('d',false);
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleDeaf : public Module
{
	User_d* m1;
 public:
	ModuleDeaf(InspIRCd* Me)
		: Module::Module(Me)
	{
		m1 = new User_d(ServerInstance);
		ServerInstance->AddMode(m1, 'd');
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnBuildExemptList] = 1;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return PreText(user, dest, target_type, text, status, exempt_list);
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return PreText(user, dest, target_type, text, status, exempt_list);
	}

	virtual void OnBuildExemptList(MessageType message_type, chanrec* chan, userrec* sender, char status, CUList &exempt_list)
	{
		CUList *ulist;
		switch (status)
		{
			case '@':
				ulist = chan->GetOppedUsers();
				break;
			case '%':
				ulist = chan->GetHalfoppedUsers();
				break;
			case '+':
				ulist = chan->GetVoicedUsers();
				break;
			default:
				ulist = chan->GetUsers();
				break;
		}

		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			if (IS_LOCAL(i->second))
			{
				if (i->second->IsModeSet('d'))
				{
					exempt_list[i->second] = i->second;
				}
			}
		}
	}

	virtual int PreText(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* chan = (chanrec*)dest;
			if (chan)
			{
				this->OnBuildExemptList(chan, status, exempt_list);
			}
		}
		return 0;
	}

	virtual ~ModuleDeaf()
	{
		ServerInstance->Modes->DelMode(m1);
		DELETE(m1);
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}

};


class ModuleDeafFactory : public ModuleFactory
{
 public:
	ModuleDeafFactory()
	{
	}
	
	~ModuleDeafFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleDeaf(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleDeafFactory;
}

