/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2016 Attila Molnar <attilamolnar@hush.com>
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

enum
{
	// InspIRCd-specific.
	ERR_TOPICLOCK = 744
};

class CommandSVSTOPIC final
	: public Command
{
public:
	CommandSVSTOPIC(Module* Creator)
		: Command(Creator, "SVSTOPIC", 1, 4)
	{
		access_needed = CmdAccess::SERVER;
		allow_empty_last_param = true;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!user->server->IsService())
		{
			// Ulines only
			return CmdResult::FAILURE;
		}

		auto chan = ServerInstance->Channels.Find(parameters[0]);
		if (!chan)
			return CmdResult::FAILURE;

		if (parameters.size() == 4)
		{
			// 4 parameter version, set all topic data on the channel to the ones given in the parameters
			time_t topicts = ConvToNum<time_t>(parameters[1]);
			if (!topicts)
			{
				ServerInstance->Logs.Normal(MODNAME, "Received SVSTOPIC with a 0 topicts, dropped.");
				return CmdResult::INVALID;
			}

			chan->SetTopic(user, parameters[3], topicts, &parameters[2]);
		}
		else
		{
			// 1 parameter version, nuke the topic
			chan->SetTopic(user, std::string(), 0);
			chan->setby.clear();
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleTopicLock final
	: public Module
{
private:
	CommandSVSTOPIC cmd;
	BoolExtItem topiclock;

public:
	ModuleTopicLock()
		: Module(VF_VENDOR | VF_COMMON, "Allows services to lock the channel topic so that it can not be changed.")
		, cmd(this)
		, topiclock(this, "topiclock", ExtensionType::CHANNEL)
	{
	}

	ModResult OnPreTopicChange(User* user, Channel* chan, const std::string& topic) override
	{
		// Only fired for local users currently, but added a check anyway
		if ((IS_LOCAL(user)) && (topiclock.Get(chan)))
		{
			user->WriteNumeric(ERR_TOPICLOCK, chan->name, "TOPIC cannot be changed due to topic lock being active on the channel");
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleTopicLock)
