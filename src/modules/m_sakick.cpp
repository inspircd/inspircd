/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/** Handle /SAKICK
 */
class CommandSakick : public Command
{
 public:
	CommandSakick(Module* Creator) : Command(Creator,"SAKICK", 2, 3)
	{
		flags_needed = 'o'; syntax = "<channel> <nick> [reason]";
		TRANSLATE3(TR_TEXT, TR_NICK, TR_TEXT);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[1]);
		Channel* channel = ServerInstance->FindChan(parameters[0]);

		if ((dest) && (dest->registered == REG_ALL) && (channel))
		{
			const std::string& reason = (parameters.size() > 2) ? parameters[2] : dest->nick;

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

			/* For local clients, directly kick them. For remote clients,
			 * just return CMD_SUCCESS knowing the protocol module will route the SAKICK to the user's
			 * local server and that will kick them instead.
			 */
			if (IS_LOCAL(dest))
			{
				// Target is on this server, kick them and send the snotice
				channel->KickUser(ServerInstance->FakeClient, dest, reason);
				ServerInstance->SNO->WriteGlobalSno('a', user->nick + " SAKICKed " + dest->nick + " on " + channel->name);
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
		return ROUTE_OPT_UCAST(parameters[1]);
	}
};

class ModuleSakick : public Module
{
	CommandSakick cmd;
 public:
	ModuleSakick()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a SAKICK command", VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleSakick)
