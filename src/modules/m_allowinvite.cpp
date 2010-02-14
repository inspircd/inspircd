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
		if (!ServerInstance->Modes->AddMode(&ni))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreInvite, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	virtual ModResult OnUserPreInvite(User* user,User* dest,Channel* channel, time_t timeout)
	{
		if (IS_LOCAL(user))
		{
			ModResult res = channel->GetExtBanStatus(user, 'A');
			if (res == MOD_RES_DENY)
			{
				// Matching extban, explicitly deny /invite
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You are banned from using INVITE", user->nick.c_str(), channel->name.c_str());
				return res;
			}
			if (channel->IsModeSet(&ni) || res == MOD_RES_ALLOW)
			{
				// Explicitly allow /invite
				return MOD_RES_ALLOW;
			}
		}

		return MOD_RES_PASSTHRU;
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
