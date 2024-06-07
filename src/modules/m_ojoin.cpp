/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

#define NETWORK_VALUE 9000000

class CommandOjoin final
	: public SplitCommand
{
public:
	bool active;
	bool notice;
	bool op;
	ModeHandler* npmh;
	ChanModeReference opmode;
	CommandOjoin(Module* parent, ModeHandler& mode)
		: SplitCommand(parent, "OJOIN", 1)
		, npmh(&mode)
		, opmode(parent, "op")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<channel>" };
		active = false;
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		// Make sure the channel name is allowable.
		if (!ServerInstance->Channels.IsChannel(parameters[0]))
		{
			user->WriteNumeric(ERR_BADCHANMASK, parameters[0], "Invalid channel name");
			return CmdResult::FAILURE;
		}

		active = true;
		// override is false because we want OnUserPreJoin to run
		Membership* memb = Channel::JoinUser(user, parameters[0], false);
		active = false;

		if (memb)
		{
			ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used OJOIN to join "+memb->chan->name);

			if (notice)
				memb->chan->WriteRemoteNotice(user->nick + " joined on official network business.");
		}
		else
		{
			Channel* channel = ServerInstance->Channels.Find(parameters[0]);
			if (!channel)
				return CmdResult::FAILURE;

			ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used OJOIN in "+parameters[0]);
			// they're already in the channel
			Modes::ChangeList changelist;
			changelist.push_add(npmh, user->nick);
			if (op && opmode)
				changelist.push_add(*opmode, user->nick);
			ServerInstance->Modes.Process(ServerInstance->FakeClient, channel, nullptr, changelist);
		}
		return CmdResult::SUCCESS;
	}
};

/** channel mode +Y
 */
class NetworkPrefix final
	: public PrefixMode
{
public:
	NetworkPrefix(Module* parent, char NPrefix)
		: PrefixMode(parent, "official-join", 'Y', NETWORK_VALUE, NPrefix)
	{
		ranktoset = ranktounset = std::numeric_limits<ModeHandler::Rank>::max();
	}

	ModResult AccessCheck(User* source, Channel* channel, Modes::Change& change) override
	{
		auto* theuser = ServerInstance->Users.Find(change.param);
		// remove own privs?
		if (source == theuser && !change.adding)
			return MOD_RES_ALLOW;

		return MOD_RES_PASSTHRU;
	}
};

class ModuleOjoin final
	: public Module
{
private:
	NetworkPrefix np;
	CommandOjoin mycommand;

public:

	ModuleOjoin()
		: Module(VF_VENDOR, "Adds the /OJOIN command which allows server operators to join a channel and receive the server operator-only Y (official-join) channel prefix mode.")
		, np(this, ServerInstance->Config->ConfValue("ojoin")->getCharacter("prefix"))
		, mycommand(this, np)
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (mycommand.active)
		{
			privs += np.GetModeChar();
			if (mycommand.op && mycommand.opmode)
				privs += mycommand.opmode->IsPrefixMode()->GetPrefix();
			return MOD_RES_ALLOW;
		}

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& Conf = ServerInstance->Config->ConfValue("ojoin");
		mycommand.notice = Conf->getBool("notice", true);
		mycommand.op = Conf->getBool("op", true);
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason) override
	{
		// Don't do anything if they're not +Y
		if (!memb->HasMode(&np))
			return MOD_RES_PASSTHRU;

		// Let them do whatever they want to themselves.
		if (source == memb->user)
			return MOD_RES_PASSTHRU;

		source->WriteNumeric(ERR_RESTRICTED, memb->chan->name, "Can't kick "+memb->user->nick+" as they're on official network business.");
		return MOD_RES_DENY;
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnUserPreJoin, PRIORITY_FIRST);
	}
};

MODULE_INIT(ModuleOjoin)
