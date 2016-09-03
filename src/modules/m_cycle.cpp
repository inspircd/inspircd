/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Handle /CYCLE
 */
class CommandCycle : public SplitCommand
{
 public:
	CommandCycle(Module* Creator)
		: SplitCommand(Creator, "CYCLE", 1)
	{
		Penalty = 3; syntax = "<channel> :[reason]";
	}

	CmdResult HandleLocal(const std::vector<std::string> &parameters, LocalUser* user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		std::string reason = "Cycling";

		if (parameters.size() > 1)
		{
			/* reason provided, use it */
			reason = reason + ": " + parameters[1];
		}

		if (!channel)
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, parameters[0], "No such channel");
			return CMD_FAILURE;
		}

		if (channel->HasUser(user))
		{
			if (channel->GetPrefixValue(user) < VOICE_VALUE && channel->IsBanned(user))
			{
				// User is banned, send an error and don't cycle them
				user->WriteNotice("*** You may not cycle, as you are banned on channel " + channel->name);
				return CMD_FAILURE;
			}

			channel->PartUser(user, reason);
			Channel::JoinUser(user, parameters[0], true);

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, channel->name, "You're not on that channel");
		}

		return CMD_FAILURE;
	}
};


class ModuleCycle : public Module
{
	CommandCycle cmd;

 public:
	ModuleCycle()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides command CYCLE, acts as a server-side HOP command to part and rejoin a channel.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCycle)
