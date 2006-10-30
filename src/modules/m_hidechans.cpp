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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for hiding oper status with user mode +I */

/** Handles user mode +I
 */
class HideChans : public ModeHandler
{
 public:
	HideChans(InspIRCd* Instance) : ModeHandler(Instance, 'I', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('I'))
			{
				dest->SetMode('I',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('I'))
			{
				dest->SetMode('I',false);
				return MODEACTION_ALLOW;
			}
		}
		
		return MODEACTION_DENY;
	}
};

class ModuleHideChans : public Module
{
	
	HideChans* hm;
 public:
	ModuleHideChans(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		hm = new HideChans(ServerInstance);
		ServerInstance->AddMode(hm, 'I');
	}

	void Implements(char* List)
	{
		List[I_OnWhoisLine] = 1;
	}
	
	virtual ~ModuleHideChans()
	{
		ServerInstance->Modes->DelMode(hm);
		DELETE(hm);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	int OnWhoisLine(userrec* user, userrec* dest, int &numeric, std::string &text)
	{
		/* Dont display channels if they have +I set and the
		 * person doing the WHOIS is not an oper
		 */
		return ((!*user->oper) && (numeric == 319) && dest->IsModeSet('I'));
	}
};

class ModuleHideChansFactory : public ModuleFactory
{
 public:
	ModuleHideChansFactory()
	{
	}
	
	~ModuleHideChansFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleHideChans(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleHideChansFactory;
}
