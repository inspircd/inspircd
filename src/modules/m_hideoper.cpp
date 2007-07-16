/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for hiding oper status with user mode +H */

/** Handles user mode +B
 */
class HideOper : public ModeHandler
{
 public:
	HideOper(InspIRCd* Instance) : ModeHandler(Instance, 'H', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (source != dest)
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
		: Module(Me)
	{
		
		hm = new HideOper(ServerInstance);
		if (!ServerInstance->AddMode(hm, 'H'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnWhoisLine] = 1;
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
		return ((!IS_OPER(user)) && (numeric == 313) && dest->IsModeSet('H'));
	}
};


MODULE_INIT(ModuleHideOper)
