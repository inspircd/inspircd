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

/* $ModDesc: Provides command CYCLE, acts as a server-side HOP command to part and rejoin a channel. */

/** Handle /CYCLE
 */
class CommandCycle : public Command
{
 public:
	CommandCycle(Module* Creator) : Command(Creator,"CYCLE", 1)
	{
		Penalty = 3; syntax = "<channel> :[reason]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		Channel* channel = ServerInstance->FindChan(parameters[0]);
		std::string reason = ConvToStr("Cycling");

		if (parameters.size() > 1)
		{
			/* reason provided, use it */
			reason = reason + ": " + parameters[1];
		}

		if (!channel)
		{
			user->WriteNumeric(403, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (channel->HasUser(user))
		{
			/*
			 * technically, this is only ever sent locally, but pays to be safe ;p
			 */
			if (IS_LOCAL(user))
			{
				if (channel->GetPrefixValue(user) < VOICE_VALUE && channel->IsBanned(user))
				{
					/* banned, boned. drop the message. */
					user->WriteServ("NOTICE "+user->nick+" :*** You may not cycle, as you are banned on channel " + channel->name);
					return CMD_FAILURE;
				}

				channel->PartUser(user, reason);

				Channel::JoinUser(user, parameters[0].c_str(), true, "", false, ServerInstance->Time());
			}

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNumeric(442, "%s %s :You're not on that channel", user->nick.c_str(), channel->name.c_str());
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

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleCycle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides command CYCLE, acts as a server-side HOP command to part and rejoin a channel.", VF_VENDOR);
	}

};

MODULE_INIT(ModuleCycle)
