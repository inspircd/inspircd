/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021, 2024 Sadie Powell <sadie@witchery.services>
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

namespace
{
	size_t NextToken(const std::string& line, size_t start)
	{
		if (start == std::string::npos || start + 1 > line.length())
			return std::string::npos;

		return line.find(' ', start + 1);
	}
}

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
	{
		cmdstart = NextToken(line, 0);
		if (cmdstart != std::string::npos)
			cmdstart++;
	}

	if (line[cmdstart] == ':') // Skip the prefix.
	{
		cmdstart = NextToken(line, cmdstart);
		if (cmdstart != std::string::npos)
			cmdstart++;
	}

	// Find the end of the command.
	size_t cmdend = NextToken(line, cmdstart);
	if (cmdend == std::string::npos)
		cmdend = line.size() - 1;

	std::string command(line, cmdstart, cmdend - cmdstart);
	if (proto_version == PROTO_INSPIRCD_4)
	{
		if (irc::equals(command, "FJOIN"))
		{
			// :<sid> FJOIN <chan> <chants> <modes> :[<modes>],<uuid>:<membid>/<joined> [<modes>],<uuid>:<membid>/<joined>
			//                                                                ^^^^^^^^^ New in 120
			const auto chanend = NextToken(line, cmdend);
			const auto chantsend = NextToken(line, chanend);
			const auto modesend = NextToken(line, chantsend);
			if (modesend != std::string::npos)
			{
				auto pos = modesend;
				while (pos != std::string::npos)
				{
					auto next = NextToken(line, pos);
					auto slash = line.find('/', pos);
					if (slash != std::string::npos)
						line.erase(slash, next - slash);
					pos = next;
				}
			}
		}
		else if (irc::equals(command, "IJOIN"))
		{
			// :<uuid> IJOIN <chan> <membid> <joints> [<chants> <modes>]
			//                              ^^^^^^^^ New in 1207
			const auto chanend = NextToken(line, cmdend);
			const auto membidend = NextToken(line, chanend);
			const auto jointsend = NextToken(line, membidend);
			if (jointsend != std::string::npos)
				line.erase(membidend, jointsend - membidend);
		}
	}
	WriteLineInternal(line);
}

bool TreeSocket::PreProcessOldProtocolMessage(User*& who, std::string& cmd, CommandBase::Params& params)
{
	if (irc::equals(cmd, "IJOIN"))
	{
		if (params.size() < 3)
			return false; // Malformed.

		// :<uuid> IJOIN <chan> <membid> <joints> [<chants> <modes>]
		//                               ^^^^^^^^ New in 1207
		params.insert(params.begin() + 2, "0");
	}
	return true;
}
