/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <options defaulttopic="Welcome to your new channel. See #help for help.">
/// $ModDepends: core 3
/// $ModDesc: Adds support for default channel topics.


#include "inspircd.h"

class ModuleDefaultTopic : public Module
{
 private:
	std::string defaulttopic;

 public:
	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("options");
		defaulttopic = tag->getString("defaulttopic", "", 0, ServerInstance->Config->Limits.MaxTopic);
	}

	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except) CXX11_OVERRIDE
	{
		if (created && !defaulttopic.empty())
			memb->chan->SetTopic(ServerInstance->FakeClient, defaulttopic, ServerInstance->Time());
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds support for default channel topics.", VF_COMMON);
	}
};

MODULE_INIT(ModuleDefaultTopic)
