/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 delthas
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
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
#include "modules/ircv3.h"
#include "modules/ircv3_replies.h"
#include "modules/monitor.h"

class CommandSetName final
	: public SplitCommand
{
private:
	IRCv3::Replies::Fail fail;

public:
	Cap::Capability cap;
	bool notifyopers;

	CommandSetName(Module* Creator)
		: SplitCommand(Creator, "SETNAME", 1, 1)
		, fail(Creator)
		, cap(Creator, "setname")
	{
		syntax = { ":<realname>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (parameters[0].size() > ServerInstance->Config->Limits.MaxReal)
		{
			fail.SendIfCap(user, &cap, this, "INVALID_REALNAME", "Real name is too long");
			return CmdResult::FAILURE;
		}

		user->ChangeRealName(parameters[0]);
		if (notifyopers)
			ServerInstance->SNO.WriteGlobalSno('a', "{} used SETNAME to change their real name to '{}'",
				user->nick, parameters[0]);
		return CmdResult::SUCCESS;
	}
};

class ModuleSetName final
	: public Module
{
private:
	CommandSetName cmd;
	ClientProtocol::EventProvider setnameevprov;
	Monitor::API monitorapi;

public:
	ModuleSetName()
		: Module(VF_VENDOR, "Adds the /SETNAME command which allows users to change their real name.")
		, cmd(this)
		, setnameevprov(this, "SETNAME")
		, monitorapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("setname");

		// Whether the module should only be usable by server operators.
		bool operonly = tag->getBool("operonly");
		cmd.access_needed = operonly ? CmdAccess::OPERATOR : CmdAccess::NORMAL;

		// Whether a snotice should be sent out when a user changes their real name.
		cmd.notifyopers = tag->getBool("notifyopers", !operonly);
	}

	void OnChangeRealName(User* user, const std::string& real) override
	{
		if (!(user->connected & User::CONN_NICKUSER))
			return;

		ClientProtocol::Message msg("SETNAME", user);
		msg.PushParamRef(real);
		ClientProtocol::Event protoev(setnameevprov, msg);
		IRCv3::WriteNeighborsWithCap res(user, protoev, cmd.cap, true);
		Monitor::WriteWatchersWithCap(monitorapi, user, protoev, cmd.cap, res.GetAlreadySentId());
	}
};

MODULE_INIT(ModuleSetName)
