/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/extban.h"
#include "numerichelper.h"

class ServerExtBan final
	: public ExtBan::MatchingBase
{
public:
	bool operonly;

	ServerExtBan(const WeakModulePtr& Creator)
		: ExtBan::MatchingBase(Creator, "server", 's')
	{
	}

	bool IsMatch(ListModeBase* lm, User* user, Channel* channel, const std::string& text, const ExtBan::MatchConfig& config) override
	{
		const auto* server = user->server;
		return InspIRCd::Match(operonly ? server->GetName() : server->GetPublicName(), text);
	}

	bool Validate(ListModeBase* lm, LocalUser* user, Channel* channel, std::string& text) override
	{
		if (operonly && !user->HasPrivPermission("users/auspex"))
		{
			user->WriteNumeric(Numerics::NoPrivileges(user, "your server operator account does not have the users/auspex privilege"));
			return false;
		}
		return true;
	}
};

class ModuleServerBan final
	: public Module
{
private:
	ServerExtBan extban;

public:
	ModuleServerBan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban s: (server) which check whether users are on a server matching the specified glob pattern.")
		, extban(weak_from_this())
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("serverban");
		extban.operonly = tag->getBool("operonly");
	}
};

MODULE_INIT(ModuleServerBan)
