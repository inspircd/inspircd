/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2007 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handle /SAPART
 */
class CommandSapart : public Command
{
 public:
	CommandSapart(Module* Creator) : Command(Creator,"SAPART", 2, 3)
	{
		flags_needed = 'o'; syntax = "<nick> <channel>[,<channel>] [reason]";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_TEXT);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (CommandParser::LoopCall(user, this, parameters, 1))
			return CMD_FAILURE;

		User* dest = ServerInstance->FindNick(parameters[0]);
		Channel* channel = ServerInstance->FindChan(parameters[1]);
		std::string reason;

		if ((dest) && (dest->registered == REG_ALL) && (channel))
		{
			if (parameters.size() > 2)
				reason = parameters[2];

			if (dest->server->IsULine())
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "Cannot use an SA command on a u-lined client");
				return CMD_FAILURE;
			}

			if (!channel->HasUser(dest))
			{
				user->WriteNotice("*** " + dest->nick + " is not on " + channel->name);
				return CMD_FAILURE;
			}

			/* For local clients, directly part them generating a PART message. For remote clients,
			 * just return CMD_SUCCESS knowing the protocol module will route the SAPART to the users
			 * local server and that will generate the PART instead
			 */
			if (IS_LOCAL(dest))
			{
				channel->PartUser(dest, reason);
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used SAPART to make "+dest->nick+" part "+channel->name);
			}

			return CMD_SUCCESS;
		}
		else
		{
			user->WriteNotice("*** Invalid nickname or channel");
		}

		return CMD_FAILURE;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};


class ModuleSapart : public Module
{
	CommandSapart cmd;
 public:
	ModuleSapart()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides command SAPART to force-part users from a channel.", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleSapart)
