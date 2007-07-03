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

/* $ModDesc: Provides support for hiding channels with user mode +I */

/** Handles user mode +I
 */
class HideChans : public ModeHandler
{
 public:
	HideChans(InspIRCd* Instance) : ModeHandler(Instance, 'I', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if (source != dest)
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
		: Module(Me)
	{
		
		hm = new HideChans(ServerInstance);
		if (!ServerInstance->AddMode(hm, 'I'))
			throw ModuleException("Could not add new modes!");
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
		return ((user != dest) && (!IS_OPER(user)) && (numeric == 319) && dest->IsModeSet('I'));
	}
};


MODULE_INIT(ModuleHideChans)
