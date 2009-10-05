/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style umode +B */

/** Handles user mode +B
 */
class BotMode : public SimpleUserModeHandler
{
 public:
	BotMode(Module* Creator) : SimpleUserModeHandler(Creator, 'B') { }
};

class ModuleBotMode : public Module
{
	BotMode bm;
 public:
	ModuleBotMode()
		: bm(this)
	{
		if (!ServerInstance->Modes->AddMode(&bm))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual ~ModuleBotMode()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style umode +B",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnWhois(User* src, User* dst)
	{
		if (dst->IsModeSet('B'))
		{
			ServerInstance->SendWhoisLine(src, dst, 335, std::string(src->nick)+" "+std::string(dst->nick)+" :is a bot on "+ServerInstance->Config->Network);
		}
	}

};


MODULE_INIT(ModuleBotMode)
