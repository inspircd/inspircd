/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2006 Craig Edwards <craigedwards@brainbox.cc>
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

typedef insp::flat_set<std::string, irc::insensitive_swo> AllowChans;

class ModuleRestrictChans : public Module
{
	AllowChans allowchans;
	UserModeReference regmode;
	bool allowregistered;

	bool CanCreateChannel(LocalUser* user, const std::string& name)
	{
		if (allowregistered && user->IsModeSet(regmode))
			return true;

		if (user->IsOper())
			return true;

		if (allowchans.count(name))
			return true;

		return false;
	}

 public:
	ModuleRestrictChans()
		: allowregistered(false)
		, regmode(this, "u_registered")
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		AllowChans newallows;
		ConfigTagList tags = ServerInstance->Config->ConfTags("allowchannel");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			const std::string name = i->second->getString("name");
			if (name.empty())
				throw ModuleException("Empty <allowchannel:name> at " + i->second->getTagLocation());

			newallows.insert(name);
		}
		allowchans.swap(newallows);

		// Global config
		ConfigTag* tag = ServerInstance->Config->ConfValue("restrictchans");
		allowregistered = tag->getBool("allowregistered", false);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		// channel does not yet exist (record is null, about to be created IF we were to allow it)
		if (!chan && !CanCreateChannel(user, cname))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Only opers may create new channels if this module is loaded", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRestrictChans)
