/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *                              
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *      
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */     

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +z */

static char* dummy;

class SSLMode : public ModeHandler
{
 public:
	SSLMode(InspIRCd* Instance) : ModeHandler(Instance, 'z', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('z'))
			{
				if (IS_LOCAL(source))
				{
					CUList* userlist = channel->GetUsers();
					for(CUList::iterator i = userlist->begin(); i != userlist->end(); i++)
					{
						if(!i->second->GetExt("ssl", dummy))
						{
							source->WriteServ("490 %s %s :all members of the channel must be connected via SSL", source->nick, channel->name);
							return MODEACTION_DENY;
						}
					}
				}
				channel->SetMode('z',true);
				return MODEACTION_ALLOW;
			}
			else
			{
				return MODEACTION_DENY;
			}
		}
		else
		{
			if (channel->IsModeSet('z'))
			{
				channel->SetMode('z',false);
				return MODEACTION_ALLOW;
			}

			return MODEACTION_DENY;
		}
	}
};

class ModuleSSLModes : public Module
{
	
	SSLMode* sslm;
	
 public:
	ModuleSSLModes(InspIRCd* Me)
		: Module::Module(Me)
	{
		

		sslm = new SSLMode(ServerInstance);
		ServerInstance->AddMode(sslm, 'z');
	}

	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = 1;
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname, std::string &privs)
	{
		if(chan && chan->IsModeSet('z'))
		{
			if(user->GetExt("ssl", dummy))
			{
				// Let them in
				return 0;
			}
			else
			{
				// Deny
				user->WriteServ( "489 %s %s :Cannot join channel (+z)", user->nick, cname);
				return 1;
			}
		}
		
		return 0;
	}

	virtual ~ModuleSSLModes()
	{
		ServerInstance->Modes->DelMode(sslm);
		DELETE(sslm);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


class ModuleSSLModesFactory : public ModuleFactory
{
 public:
	ModuleSSLModesFactory()
	{
	}
	
	~ModuleSSLModesFactory()
	{
	}
	
	virtual Module* CreateModule(InspIRCd* Me)
	{
		return new ModuleSSLModes(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSSLModesFactory;
}
