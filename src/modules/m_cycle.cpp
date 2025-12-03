/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "numerichelper.h"

class CommandCycle final
	: public SplitCommand
{
public:
	CommandCycle(Module* Creator)
		: SplitCommand(Creator, "CYCLE", 1)
	{
		penalty = 3000;
		syntax = { "<channel> [:<reason>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		auto* channel = ServerInstance->Channels.Find(parameters[0]);
		std::string reason = "Cycling";

		if (parameters.size() > 1)
		{
			/* reason provided, use it */
			reason = reason + ": " + parameters[1];
		}

		if (!channel)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
			return CmdResult::FAILURE;
		}

		if (channel->HasUser(user))
		{
			if (channel->GetPrefixValue(user) < VOICE_VALUE && channel->IsBanned(user))
			{
				// User is banned, send an error and don't cycle them
				user->WriteNumeric(ERR_BANNEDFROMCHAN, channel->name, "You may not cycle, as you are banned on channel " + channel->name);
				return CmdResult::FAILURE;
			}

			channel->PartUser(user, reason);
			Channel::JoinUser(user, parameters[0], true);

			return CmdResult::SUCCESS;
		}
		else
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, channel->name, "You're not on that channel");
		}

		return CmdResult::FAILURE;
	}
};

class ModuleCycle final
	: public Module
{
private:
	CommandCycle cmd;

public:
	ModuleCycle()
		: Module(VF_VENDOR, "Allows channel members to part and rejoin a channel without needing to worry about channel modes such as +i (inviteonly) which might prevent rejoining.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleCycle)
