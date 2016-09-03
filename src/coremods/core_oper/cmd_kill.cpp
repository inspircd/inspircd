/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "core_oper.h"

CommandKill::CommandKill(Module* parent)
	: Command(parent, "KILL", 2, 2)
{
	flags_needed = 'o';
	syntax = "<nickname> <reason>";
	TRANSLATE2(TR_CUSTOM, TR_CUSTOM);
}


/** Handle /KILL
 */
CmdResult CommandKill::Handle (const std::vector<std::string>& parameters, User *user)
{
	/* Allow comma seperated lists of users for /KILL (thanks w00t) */
	if (CommandParser::LoopCall(user, this, parameters, 0))
	{
		// If we got a colon delimited list of nicks then the handler ran for each nick,
		// and KILL commands were broadcast for remote targets.
		return CMD_FAILURE;
	}

	User *u = ServerInstance->FindNick(parameters[0]);
	if (u)
	{
		/*
		 * Here, we need to decide how to munge kill messages. Whether to hide killer, what to show opers, etc.
		 * We only do this when the command is being issued LOCALLY, for remote KILL, we just copy the message we got.
		 *
		 * This conditional is so that we only append the "Killed (" prefix ONCE. If killer is remote, then the kill
		 * just gets processed and passed on, otherwise, if they are local, it gets prefixed. Makes sense :-) -- w00t
		 */

		if (IS_LOCAL(user))
		{
			/*
			 * Moved this event inside the IS_LOCAL check also, we don't want half the network killing a user
			 * and the other half not. This would be a bad thing. ;p -- w00t
			 */
	 		ModResult MOD_RESULT;
			FIRST_MOD_RESULT(OnKill, MOD_RESULT, (user, u, parameters[1]));

			if (MOD_RESULT == MOD_RES_DENY)
				return CMD_FAILURE;

			killreason = "Killed (";
			if (!ServerInstance->Config->HideKillsServer.empty())
			{
				// hidekills is on, use it
				killreason += ServerInstance->Config->HideKillsServer;
			}
			else
			{
				// hidekills is off, do nothing
				killreason += user->nick;
			}

			killreason += " (" + parameters[1] + "))";
		}
		else
		{
			/* Leave it alone, remote server has already formatted it */
			killreason.assign(parameters[1], 0, ServerInstance->Config->Limits.MaxQuit);
		}

		/*
		 * Now we need to decide whether or not to send a local or remote snotice. Currently this checking is a little flawed.
		 * No time to fix it right now, so left a note. -- w00t
		 */
		if (!IS_LOCAL(u))
		{
			// remote kill
			if ((!ServerInstance->Config->HideULineKills) || (!user->server->IsULine()))
				ServerInstance->SNO->WriteToSnoMask('K', "Remote kill by %s: %s (%s)", user->nick.c_str(), u->GetFullRealHost().c_str(), parameters[1].c_str());
			this->lastuuid = u->uuid;
		}
		else
		{
			// local kill
			/*
			 * XXX - this isn't entirely correct, servers A - B - C, oper on A, client on C. Oper kills client, A and B will get remote kill
			 * snotices, C will get a local kill snotice. this isn't accurate, and needs fixing at some stage. -- w00t
			 */
			if ((!ServerInstance->Config->HideULineKills) || (!user->server->IsULine()))
			{
				if (IS_LOCAL(user))
					ServerInstance->SNO->WriteGlobalSno('k',"Local Kill by %s: %s (%s)", user->nick.c_str(), u->GetFullRealHost().c_str(), parameters[1].c_str());
				else
					ServerInstance->SNO->WriteToSnoMask('k',"Local Kill by %s: %s (%s)", user->nick.c_str(), u->GetFullRealHost().c_str(), parameters[1].c_str());
			}

			ServerInstance->Logs->Log("KILL", LOG_DEFAULT, "LOCAL KILL: %s :%s!%s!%s (%s)", u->nick.c_str(), ServerInstance->Config->ServerName.c_str(), user->dhost.c_str(), user->nick.c_str(), parameters[1].c_str());

			u->Write(":%s KILL %s :%s!%s!%s (%s)", ServerInstance->Config->HideKillsServer.empty() ? user->GetFullHost().c_str() : ServerInstance->Config->HideKillsServer.c_str(),
					u->nick.c_str(),
					ServerInstance->Config->ServerName.c_str(),
					ServerInstance->Config->HideKillsServer.empty() ? user->dhost.c_str() : ServerInstance->Config->HideKillsServer.c_str(),
					ServerInstance->Config->HideKillsServer.empty() ? user->nick.c_str() : ServerInstance->Config->HideKillsServer.c_str(),
					parameters[1].c_str());

			this->lastuuid.clear();
		}

		// send the quit out
		ServerInstance->Users->QuitUser(u, killreason);
	}
	else
	{
		user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandKill::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	// FindNick() doesn't work here because we quit the target user in Handle() which
	// removes it from the nicklist, so we check lastuuid: if it's empty then this KILL
	// was for a local user, otherwise it contains the uuid of the user who was killed.
	if (lastuuid.empty())
		return ROUTE_LOCALONLY;
	return ROUTE_BROADCAST;
}


void CommandKill::EncodeParameter(std::string& param, int index)
{
	// Manually translate the nick -> uuid (see above), and also the reason (params[1])
	// because we decorate it if the oper is local and want remote servers to see the
	// decorated reason not the original.
	param = ((index == 0) ? lastuuid : killreason);
}
