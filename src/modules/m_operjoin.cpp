/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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

class ModuleOperjoin final
	: public Module
{
private:
	std::vector<std::string> operChans;
	bool override;

public:
	ModuleOperjoin()
		: Module(VF_VENDOR, "Allows the server administrator to force server operators to join one or more channels when logging into their server operator account.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("operjoin");

		override = tag->getBool("override", false);
		irc::commasepstream ss(tag->getString("channel"));
		operChans.clear();

		for (std::string channame; ss.GetToken(channame); )
			operChans.push_back(channame);
	}

	void OnPostOperLogin(User* user, bool automatic) override
	{
		LocalUser* localuser = IS_LOCAL(user);
		if (!localuser)
			return;

		for (const auto& operchan : operChans)
		{
			if (ServerInstance->Channels.IsChannel(operchan))
				Channel::JoinUser(localuser, operchan, override);
		}

		irc::commasepstream ss(localuser->oper->GetConfig()->getString("autojoin"));
		for (std::string channame; ss.GetToken(channame); )
		{
			if (ServerInstance->Channels.IsChannel(channame))
				Channel::JoinUser(localuser, channame, override);
		}
	}
};

MODULE_INIT(ModuleOperjoin)
