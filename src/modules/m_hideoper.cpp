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

/* $ModDesc: Provides support for hiding oper status with user mode +H */

/** Handles user mode +B
 */
class HideOper : public ModeHandler
{
 public:
	HideOper(InspIRCd* Instance) : ModeHandler(Instance, 'H', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		if (adding)
		{
			if (!dest->IsModeSet('H'))
			{
				dest->SetMode('H',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('H'))
			{
				dest->SetMode('H',false);
				return MODEACTION_ALLOW;
			}
		}
		
		return MODEACTION_DENY;
	}
};

class ModuleHideOper : public Module
{
	
	HideOper* hm;
 public:
	ModuleHideOper(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		hm = new HideOper(ServerInstance);
		ServerInstance->AddMode(hm, 'H');
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = 1;
	}
	
	virtual ~ModuleHideOper()
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
		/* Dont display numeric 313 (RPL_WHOISOPER) if they have +H set and the
		 * person doing the WHOIS is not an oper
		 */
		return ((!*user->oper) && (numeric == 313) && dest->IsModeSet('H'));
	}
};

class ModuleHideOperFactory : public ModuleFactory
{
 public:
	ModuleHideOperFactory()
	{
	}
	
	~ModuleHideOperFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleHideOper(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleHideOperFactory;
}
