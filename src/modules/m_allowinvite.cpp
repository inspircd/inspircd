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

/* $ModDesc: Provides support for channel mode +A, allowing /invite freely on a channel (and extban A to allow specific users it) */

class AllowInvite : public SimpleChannelModeHandler
{
 public:
	AllowInvite(Module* Creator) : SimpleChannelModeHandler(Creator, "allowinvite", 'A') { fixed_letter = false; }
};

class ModuleAllowInvite : public Module
{
	AllowInvite ni;
 public:

	ModuleAllowInvite() : ni(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(ni);
		Implementation eventlist[] = { I_OnPermissionCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name != "invite")
			return;

		ModResult res = perm.chan->GetExtBanStatus(perm.source, 'A');
		if (res == MOD_RES_DENY)
		{
			// Matching extban, explicitly deny /invite
			perm.result = res;
			perm.SetReason(":%s %d %s %s :You are banned from using INVITE", ServerInstance->Config->ServerName.c_str(),
				ERR_CHANOPRIVSNEEDED, perm.source->nick.c_str(), perm.chan->name.c_str());
		}
		else if (perm.chan->IsModeSet(&ni) || res == MOD_RES_ALLOW)
		{
			perm.result = MOD_RES_ALLOW;
		}
	}

	virtual ~ModuleAllowInvite()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +A, allowing /invite freely on a channel (and extban A to allow specific users it)",VF_VENDOR);
	}
};

MODULE_INIT(ModuleAllowInvite)
