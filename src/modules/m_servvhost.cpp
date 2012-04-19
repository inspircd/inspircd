/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: ircd vhosting. */

class ModuleServVHost : public Module
{
 public:
	StringExtItem cli_name, srv_name;
	ModuleServVHost() : cli_name(EXTENSIBLE_USER, "usercmd_host", this), srv_name(EXTENSIBLE_USER, "usercmd_server", this) {}

	void init()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnSetConnectClass };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("IRCd virtual host matching for connect blocks",VF_VENDOR);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string&)
	{
		if (command != "USER" || !validated)
			return MOD_RES_PASSTHRU;
		srv_name.set(user, parameters[1]);
		cli_name.set(user, parameters[2]);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		std::string sname = myclass->config->getString("servername");
		std::string* myname = srv_name.get(user);
		if (!sname.empty() && myname && !InspIRCd::Match(*myname, sname))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleServVHost)
