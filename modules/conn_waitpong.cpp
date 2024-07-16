/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
	bool sendsnotice;
	bool killonbadreply;
	StringExtItem ext;

public:
	ModuleWaitPong()
		: Module(VF_VENDOR, "Requires all clients to respond to a PING request before they can fully connect.")
		, ext(this, "waitpong-cookie", ExtensionType::USER)
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
		std::string pingrpl = ServerInstance->GenRandomStr(10);
		{
			ClientProtocol::Messages::Ping pingmsg(pingrpl);
			user->Send(ServerInstance->GetRFCEvents().ping, pingmsg);
		}

		if(sendsnotice)
			user->WriteNotice("*** If you are having problems connecting due to connection timeouts type /quote PONG " + pingrpl + " or /raw PONG " + pingrpl + " now.");

		ext.Set(user, pingrpl);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if (command == "PONG")
		{
			std::string* pingrpl = ext.Get(user);

			if (pingrpl)
			{
				if (!parameters.empty() && *pingrpl == parameters[0])
				{
					ext.Unset(user);
					return MOD_RES_DENY;
				}
				else
				{
					if(killonbadreply)
						ServerInstance->Users.QuitUser(user, "Incorrect ping reply for connection");
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		return ext.Get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleWaitPong)
