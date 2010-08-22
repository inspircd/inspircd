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

/* $ModDesc: Forces users to join the specified channel(s) on connect */

class ModuleConnJoin : public Module
{
	public:
		void init()
		{
			Implementation eventlist[] = { I_OnPostConnect };
			ServerInstance->Modules->Attach(eventlist, this, 1);
		}

		void Prioritize()
		{
			ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
		}

		Version GetVersion()
		{
			return Version("Forces users to join the specified channel(s) on connect", VF_VENDOR);
		}

		void OnPostConnect(User* user)
		{
			if (!IS_LOCAL(user))
				return;

			std::string chanlist = ServerInstance->Config->GetTag("autojoin")->getString("channel");
			chanlist = IS_LOCAL(user)->MyClass->GetConfig("autojoin", chanlist);

			irc::commasepstream chans(chanlist);
			std::string chan;

			while (chans.GetToken(chan))
			{
				if (ServerInstance->IsChannel(chan.c_str(), ServerInstance->Config->Limits.ChanMax))
					Channel::JoinUser(user, chan.c_str(), false, "", false, ServerInstance->Time());
			}
		}
};


MODULE_INIT(ModuleConnJoin)
