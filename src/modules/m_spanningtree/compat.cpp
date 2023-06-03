/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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
#include "main.h"

void TreeSocket::WriteLine(const std::string& original_line)
{
	if (LinkState != CONNECTED || proto_version == PROTO_NEWEST)
	{
		// Don't translate connections which have negotiated PROTO_NEWEST or
		// where the protocol hasn't been negotiated yet.
		WriteLineInternal(original_line);
		return;
	}

	std::string line = original_line;
	size_t cmdstart = 0;

	if (line[0] == '@') // Skip the tags.
		cmdstart = line.find(' ');

	if (line[cmdstart + 1] == ':') // Skip the prefix.
		cmdstart = line.find(' ', cmdstart + 1);

	// Find the end of the command.
	size_t cmdend = line.find(' ', cmdstart + 1);
	if (cmdend == std::string::npos)
		cmdend = line.size();

	std::string command(line, cmdstart + 1, cmdend - cmdstart - 1);
	if (proto_version == PROTO_INSPIRCD_3)
	{
		if (irc::equals(command, "FRHOST"))
		{
			// FRHOST was introduced in PROTO_INSPIRCD_4; drop it.
			return;
		}
		else if (irc::equals(command, "SQUERY"))
		{
			// SQUERY was introduced in PROTO_INSPIRCD_4; convert to PRIVMSG.
			line.replace(cmdstart + 1, cmdend - cmdstart - 1, "PRIVMSG");
		}
	}

	WriteLineInternal(line);
}
