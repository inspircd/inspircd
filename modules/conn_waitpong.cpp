/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2012, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "clientprotocolmsg.h"
#include "extension.h"

class ModuleWaitPong final
	: public Module
{
private:
	bool sendsnotice;
	bool killonbadreply;
	StringExtItem ext;

public:
	ModuleWaitPong()
		: Module(VF_VENDOR, "Requires all clients to respond to a PING request before they can fully connect.")
		, ext(weak_from_this(), "waitpong-cookie", ExtensionType::USER)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("waitpong");
		sendsnotice = tag->getBool("sendsnotice", false);
		killonbadreply = tag->getBool("killonbadreply", true);
	}

	ModResult OnUserRegister(LocalUser* user) override
	{
		const auto pingcookie = ServerInstance->GenRandomStr(16);
		ext.Set(user, pingcookie);

		ClientProtocol::Messages::Ping pingmsg(pingcookie);
		user->Send(ServerInstance->GetRFCEvents().ping, pingmsg);

		if (sendsnotice)
		{
			user->WriteNotice("*** If you are having problems connecting due to connection timeouts type `/QUOTE PONG {}` or `/RAW PONG {}` now.",
				pingcookie, pingcookie);
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (command != "PONG")
			return MOD_RES_PASSTHRU; // We don't care about this command.

		const auto* pingcookie = ext.Get(user);
		if (!pingcookie)
			return MOD_RES_PASSTHRU; // User has responded with their ping cookie.

		if (!parameters.empty() && *pingcookie == parameters[0])
			ext.Unset(user); // The user specified the right ping cookie.

		else if (killonbadreply)
			ServerInstance->Users.QuitUser(user, "Incorrect ping reply for connection");

		return MOD_RES_DENY;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return ext.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleWaitPong)
