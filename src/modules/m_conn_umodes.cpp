/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

class ModuleModesOnConnect : public Module
{
 public:
	void Prioritize() CXX11_OVERRIDE
	{
		// for things like +x on connect, important, otherwise we have to resort to config order (bleh) -- w00t
		ServerInstance->Modules->SetPriority(this, I_OnUserConnect, PRIORITY_FIRST);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Sets (and unsets) modes on users when they connect", VF_VENDOR);
	}

	void OnUserConnect(LocalUser* user) CXX11_OVERRIDE
	{
		// Backup and zero out the disabled usermodes, so that we can override them here.
		char save[64];
		memcpy(save, ServerInstance->Config->DisabledUModes,
				sizeof(ServerInstance->Config->DisabledUModes));
		memset(ServerInstance->Config->DisabledUModes, 0, 64);

		ConfigTag* tag = user->MyClass->config;
		std::string ThisModes = tag->getString("modes");
		if (!ThisModes.empty())
		{
			std::string buf;
			std::stringstream ss(ThisModes);

			std::vector<std::string> modes;
			modes.push_back(user->nick);

			// split ThisUserModes into modes and mode params
			while (ss >> buf)
				modes.push_back(buf);

			ServerInstance->Parser.CallHandler("MODE", modes, user);
		}

		memcpy(ServerInstance->Config->DisabledUModes, save, 64);
	}
};

MODULE_INIT(ModuleModesOnConnect)
