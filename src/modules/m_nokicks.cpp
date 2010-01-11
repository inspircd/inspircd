/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class NoKicks : public SimpleChannelModeHandler
{
 public:
	NoKicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nokick", 'Q') { }
};

class ModuleNoKicks : public Module
{
	NoKicks nk;

 public:
	ModuleNoKicks()
		: nk(this)
	{
		if (!ServerInstance->Modes->AddMode(&nk))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreKick, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('Q');
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string &reason)
	{
		if (!memb->chan->GetExtBanStatus(source, 'Q').check(!memb->chan->IsModeSet('Q')))
		{
			if ((ServerInstance->ULine(source->nick.c_str())) || ServerInstance->ULine(source->server))
			{
				// ulines can still kick with +Q in place
				return MOD_RES_PASSTHRU;
			}
			else
			{
				// nobody else can (not even opers with override, and founders)
				source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Can't kick user %s from channel (+Q set)",source->nick.c_str(), memb->chan->name.c_str(), memb->user->nick.c_str());
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	~ModuleNoKicks()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +Q", VF_COMMON | VF_VENDOR);
	}
};


MODULE_INIT(ModuleNoKicks)
