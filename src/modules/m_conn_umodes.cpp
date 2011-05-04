/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Sets (and unsets) modes on users when they connect */

class ModuleModesOnConnect : public Module
{
 public:
	ModuleModesOnConnect() {}

	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserConnect, this);
	}

	void Prioritize()
	{
		// for things like +x on connect, important, otherwise we have to resort to config order (bleh) -- w00t
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_FIRST);
	}

	Version GetVersion()
	{
		return Version("Sets (and unsets) modes on users when they connect", VF_VENDOR);
	}

	void OnUserConnect(LocalUser* user)
	{
		std::string ThisModes = user->MyClass->GetConfig("modes");
		if (!ThisModes.empty())
		{
			std::string buf;
			std::stringstream ss(ThisModes);

			std::vector<std::string> modes;
			modes.push_back(user->nick);

			// split ThisUserModes into modes and mode params
			while (ss >> buf)
				modes.push_back(buf);

			ServerInstance->SendMode(modes, ServerInstance->FakeClient);
		}
	}
};

MODULE_INIT(ModuleModesOnConnect)
