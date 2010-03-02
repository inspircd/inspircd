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
	NoKicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nokick", 'Q') { fixed_letter = false; }
};

class ModuleNoKicks : public Module
{
	NoKicks nk;

 public:
	ModuleNoKicks() : nk(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(nk);
		Implementation eventlist[] = { I_OnChannelPermissionCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('Q');
	}

	void OnChannelPermissionCheck(User* user,Channel* chan, PermissionData& perm)
	{
		if (perm.name == "kick" && !chan->GetExtBanStatus(user, 'Q').check(!chan->IsModeSet(&nk)))
		{
			perm.SetReason(":%s %d %s %s :Can't kick in channel (+Q set)", ServerInstance->Config->ServerName.c_str(),
				ERR_CHANOPRIVSNEEDED, user->nick.c_str(), chan->name.c_str());
			perm.result = MOD_RES_DENY;
		}
	}

	~ModuleNoKicks()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +Q", VF_VENDOR);
	}
};


MODULE_INIT(ModuleNoKicks)
