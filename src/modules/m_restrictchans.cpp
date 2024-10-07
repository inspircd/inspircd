/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018-2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/account.h"

typedef insp::flat_set<std::string, irc::insensitive_swo> AllowChans;

class ModuleRestrictChans final
	: public Module
{
private:
	Account::API accountapi;
	AllowChans allowchans;
	bool allowregistered = false;

	bool CanCreateChannel(LocalUser* user, const std::string& name)
	{
		if (allowregistered && accountapi && accountapi->GetAccountName(user))
			return true;

		if (user->HasPrivPermission("channels/restricted-create"))
			return true;

		for (const auto& allowchan : allowchans)
		{
			if (InspIRCd::Match(name, allowchan))
				return true;
		}

		return false;
	}

public:
	ModuleRestrictChans()
		: Module(VF_VENDOR, "Prevents unprivileged users from creating new channels.")
		, accountapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		AllowChans newallows;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("allowchannel"))
		{
			const std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException(this, "Empty <allowchannel:name> at " + tag->source.str());

			newallows.insert(name);
		}
		allowchans.swap(newallows);

		// Global config
		const auto& tag = ServerInstance->Config->ConfValue("restrictchans");
		allowregistered = tag->getBool("allowregistered", false);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		// channel does not yet exist (record is null, about to be created IF we were to allow it)
		if (!override && !chan && !CanCreateChannel(user, cname))
		{
			const auto* reason = allowregistered ? "logged into an account" : "a server operator";
			user->WriteNumeric(ERR_RESTRICTED, cname, INSP_FORMAT("You must be {} to create new channels.", reason));
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleRestrictChans)
