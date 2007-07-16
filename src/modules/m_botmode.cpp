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
#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "configreader.h"

/* $ModDesc: Provides support for unreal-style umode +B */

/** Handles user mode +B
 */
class BotMode : public ModeHandler
{
 public:
	BotMode(InspIRCd* Instance) : ModeHandler(Instance, 'B', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('B'))
			{
				dest->SetMode('B',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('B'))
			{
				dest->SetMode('B',false);
				return MODEACTION_ALLOW;
			}
		}
		
		return MODEACTION_DENY;
	}
};

class ModuleBotMode : public Module
{
	
	BotMode* bm;
 public:
	ModuleBotMode(InspIRCd* Me)
		: Module(Me)
	{
		
		bm = new BotMode(ServerInstance);
		if (!ServerInstance->AddMode(bm, 'B'))
			throw ModuleException("Could not add new modes!");
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = 1;
	}
	
	virtual ~ModuleBotMode()
	{
		ServerInstance->Modes->DelMode(bm);
		DELETE(bm);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnWhois(userrec* src, userrec* dst)
	{
		if (dst->IsModeSet('B'))
		{
			ServerInstance->SendWhoisLine(src, dst, 335, std::string(src->nick)+" "+std::string(dst->nick)+" :is a bot on "+ServerInstance->Config->Network);
		}
	}

};


MODULE_INIT(ModuleBotMode)
