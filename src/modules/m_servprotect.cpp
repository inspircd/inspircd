/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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

/* $ModDesc: Provides support for Austhex style +k / UnrealIRCD +S services mode */

/** Handles user mode +k
 */
class ServProtectMode : public ModeHandler
{
 public:
	ServProtectMode(InspIRCd* Instance) : ModeHandler(Instance, 'k', 0, 0, false, MODETYPE_USER, true) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		return MODEACTION_DENY;
	}

	bool NeedsOper() { return true; }
};

class ModuleServProtectMode : public Module
{
	
	ServProtectMode* bm;
 public:
	ModuleServProtectMode(InspIRCd* Me)
		: Module(Me)
	{
		
		bm = new ServProtectMode(ServerInstance);
		if (!ServerInstance->Modes->AddMode(bm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhois, I_OnKill, I_OnWhoisLine, I_OnRawMode };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	
	virtual ~ModuleServProtectMode()
	{
		ServerInstance->Modes->DelMode(bm);
		delete bm;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,2,0,0,VF_COMMON,API_VERSION);
	}

	virtual void OnWhois(User* src, User* dst)
	{
		if (dst->IsModeSet('k'))
		{
			ServerInstance->SendWhoisLine(src, dst, 310, std::string(src->nick)+" "+std::string(dst->nick)+" :is an "+ServerInstance->Config->Network+" Service");
		}
	}

	virtual int OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt, bool servermode)
	{
		if (!servermode && (mode == 'o') && !adding && chan && IS_LOCAL(user) && !ServerInstance->ULine(user->server))
		{
			User *u = ServerInstance->FindNick(param);
			if (u)
			{
				if (u->IsModeSet('k'))
				{
					user->WriteNumeric(482, "%s %s :You are not permitted to deop %s services", user->nick.c_str(), chan->name.c_str(), ServerInstance->Config->Network);
					return ACR_DENY;
				}
			}
		}
		return 0;
	}

	virtual int OnKill(User* src, User* dst, const std::string &reason)
	{
		if (src == NULL)
			return 0;

		if (dst->IsModeSet('k'))
		{
			src->WriteNumeric(485, "%s :You are not permitted to kill %s services!", src->nick.c_str(), ServerInstance->Config->Network);
			ServerInstance->SNO->WriteToSnoMask('A', std::string(src->nick)+" tried to kill service "+dst->nick+" ("+reason+")");
			return 1;
		}
		return 0;
	}

	virtual int OnWhoisLine(User* src, User* dst, int &numeric, std::string &text)
	{
		return ((src != dst) && (numeric == 319) && dst->IsModeSet('k'));
	}
};


MODULE_INIT(ModuleServProtectMode)
