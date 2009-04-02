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

/* $ModDesc: Provides support for channel mode +A, allowing /invite freely on a channel (and extban A to allow specific users it) */

class AllowInvite : public SimpleChannelModeHandler
{
 public:
	AllowInvite(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'A') { }
};

class ModuleAllowInvite : public Module
{
	AllowInvite *ni;
 public:

	ModuleAllowInvite(InspIRCd* Me) : Module(Me)
	{
		ni = new AllowInvite(ServerInstance);
		if (!ServerInstance->Modes->AddMode(ni))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreInvite, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	virtual int OnUserPreInvite(User* user,User* dest,Channel* channel, time_t timeout)
	{
		if (IS_LOCAL(user))
		{
			if (channel->GetExtBanStatus(user, 'A') == -1)
			{
				// Matching extban, explicitly deny /invite
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You are banned from using INVITE", user->nick.c_str(), channel->name.c_str());
				return 1;
			}
			if (channel->IsModeSet('A') || channel->GetExtBanStatus(user, 'A') == 1)
			{
				// Explicitly allow /invite
				return -1;
			}
		}

		return 0;
	}

	virtual ~ModuleAllowInvite()
	{
		ServerInstance->Modes->DelMode(ni);
		delete ni;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleAllowInvite)
