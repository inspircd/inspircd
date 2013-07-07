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

/** FMODE command - server mode with timestamp checks */
CmdResult CommandFMode::Handle(const std::vector<std::string>& params, User *who)
{
	time_t TS = ConvToInt(params[1]);
	if (!TS)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "*** BUG? *** TS of 0 sent to FMODE. Are some services authors smoking craq, or is it 1970 again?. Dropping link.");
		ServerInstance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FMODE with a TS of zero. Total craq, dropping link.", who->server.c_str());
		return CMD_INVALID;
	}

	/* Extract the TS value of the object, either User or Channel */
	time_t ourTS;
	if (params[0][0] == '#')
	{
		Channel* chan = ServerInstance->FindChan(params[0]);
		if (!chan)
			/* Oops, channel doesn't exist! */
			return CMD_FAILURE;

		ourTS = chan->age;
	}
	else
	{
		User* user = ServerInstance->FindUUID(params[0]);
		if (!user)
			return CMD_FAILURE;

		if (IS_SERVER(user))
			return CMD_INVALID;

		ourTS = user->age;
	}

	/* If the TS is greater than ours, we drop the mode and don't pass it anywhere.
	 */
	if (TS > ourTS)
		return CMD_FAILURE;

	/* TS is equal or less: Merge the mode changes into ours and pass on.
	 */
	std::vector<std::string> modelist;
	modelist.reserve(params.size()-1);
	/* Insert everything into modelist except the TS (params[1]) */
	modelist.push_back(params[0]);
	modelist.insert(modelist.end(), params.begin()+2, params.end());

	ModeParser::ModeProcessFlag flags = ModeParser::MODE_LOCALONLY;
	if ((TS == ourTS) && IS_SERVER(who))
		flags |= ModeParser::MODE_MERGE;

	ServerInstance->Modes->Process(modelist, who, flags);
	return CMD_SUCCESS;
}


