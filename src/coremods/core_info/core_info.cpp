/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "core_info.h"

RouteDescriptor ServerTargetCommand::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	if (!parameters.empty())
		return ROUTE_UNICAST(parameters[0]);
	return ROUTE_LOCALONLY;
}

class CoreModInfo : public Module
{
	CommandAdmin cmdadmin;
	CommandCommands cmdcommands;
	CommandInfo cmdinfo;
	CommandModules cmdmodules;
	CommandMotd cmdmotd;
	CommandTime cmdtime;
	CommandVersion cmdversion;

 public:
	CoreModInfo()
		: cmdadmin(this), cmdcommands(this), cmdinfo(this), cmdmodules(this), cmdmotd(this), cmdtime(this), cmdversion(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("admin");
		cmdadmin.AdminName = tag->getString("name");
		cmdadmin.AdminEmail = tag->getString("email", "null@example.com");
		cmdadmin.AdminNick = tag->getString("nick", "admin");
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the ADMIN, COMMANDS, INFO, MODULES, MOTD, TIME and VERSION commands", VF_VENDOR|VF_CORE);
	}
};

MODULE_INIT(CoreModInfo)
