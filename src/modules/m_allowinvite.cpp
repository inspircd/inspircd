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

	ModuleAllowInvite() : ni(this)
	{
		ServerInstance->Modules->AddService(ni);
		Implementation eventlist[] = { I_OnChannelPermissionCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	void OnChannelPermissionCheck(User* user,Channel* channel, PermissionData& perm)
	{
		if (perm.name != "invite")
			return;

		ModResult res = channel->GetExtBanStatus(user, 'A');
		if (res == MOD_RES_DENY)
		{
			// Matching extban, explicitly deny /invite
			perm.result = res;
			perm.SetReason(":%s %d %s %s :You are banned from using INVITE", ServerInstance->Config->ServerName.c_str(),
				ERR_CHANOPRIVSNEEDED, user->nick.c_str(), channel->name.c_str());
		}
		else if (channel->IsModeSet(&ni) || res == MOD_RES_ALLOW)
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
