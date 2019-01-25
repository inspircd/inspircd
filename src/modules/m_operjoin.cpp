/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2004 Christopher Hall <typobox43@gmail.com>
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

class ModuleOperjoin : public Module
{
		std::vector<std::string> operChans;
		bool override;

	public:
		void ReadConfig(ConfigStatus& status) override
		{
			ConfigTag* tag = ServerInstance->Config->ConfValue("operjoin");

			override = tag->getBool("override", false);
			irc::commasepstream ss(tag->getString("channel"));
			operChans.clear();

			for (std::string channame; ss.GetToken(channame); )
				operChans.push_back(channame);
		}

		Version GetVersion() override
		{
			return Version("Forces opers to join the specified channel(s) on oper-up", VF_VENDOR);
		}

		void OnPostOper(User* user, const std::string &opertype, const std::string &opername) override
		{
			LocalUser* localuser = IS_LOCAL(user);
			if (!localuser)
				return;

			for (std::vector<std::string>::const_iterator i = operChans.begin(); i != operChans.end(); ++i)
				if (ServerInstance->IsChannel(*i))
					Channel::JoinUser(localuser, *i, override);

			irc::commasepstream ss(localuser->oper->getConfig("autojoin"));
			for (std::string channame; ss.GetToken(channame); )
			{
				if (ServerInstance->IsChannel(channame))
					Channel::JoinUser(localuser, channame, override);
			}
		}
};

MODULE_INIT(ModuleOperjoin)
