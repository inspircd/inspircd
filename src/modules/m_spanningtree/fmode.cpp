/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/** FMODE command - server mode with timestamp checks */
CmdResult CommandFMode::Handle(const std::vector<std::string>& params, User *who)
{
	std::string sourceserv = who->server;

	std::vector<std::string> modelist;
	time_t TS = 0;
	for (unsigned int q = 0; (q < params.size()) && (q < 64); q++)
	{
		if (q == 1)
		{
			/* The timestamp is in this position.
			 * We don't want to pass that up to the
			 * server->client protocol!
			 */
			TS = atoi(params[q].c_str());
		}
		else
		{
			/* Everything else is fine to append to the modelist */
			modelist.push_back(params[q]);
		}

	}
	/* Extract the TS value of the object, either User or Channel */
	User* dst = ServerInstance->FindNick(params[0]);
	Channel* chan = NULL;
	time_t ourTS = 0;

	if (dst)
	{
		ourTS = dst->age;
	}
	else
	{
		chan = ServerInstance->FindChan(params[0]);
		if (chan)
		{
			ourTS = chan->age;
		}
		else
			/* Oops, channel doesnt exist! */
			return CMD_FAILURE;
	}

	if (!TS)
	{
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"*** BUG? *** TS of 0 sent to FMODE. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		ServerInstance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FMODE with a TS of zero. Total craq. Mode was dropped.", sourceserv.c_str());
		return CMD_INVALID;
	}

	/* TS is equal or less: Merge the mode changes into ours and pass on.
	 */
	if (TS <= ourTS)
	{
		bool merge = (TS == ourTS) && IS_SERVER(who);
		ServerInstance->Modes->Process(modelist, who, merge);
		return CMD_SUCCESS;
	}
	/* If the TS is greater than ours, we drop the mode and dont pass it anywhere.
	 */
	return CMD_FAILURE;
}


