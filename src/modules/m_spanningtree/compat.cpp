/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021, 2024-2025 Sadie Powell <sadie@witchery.services>
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

	size_t sidstart = std::string::npos;
	if (line[cmdstart] == ':') // Skip the prefix.
	{
		sidstart = cmdstart + 1;
		cmdstart = NextToken(line, cmdstart);
		if (cmdstart != std::string::npos)
			cmdstart++;
	}

	// Find the end of the command.
	size_t cmdend = NextToken(line, cmdstart);
	if (cmdend == std::string::npos)
		cmdend = line.size() - 1;

	std::string command(line, cmdstart, cmdend - cmdstart);
	if (proto_version == PROTO_INSPIRCD_3)
	{
		if (irc::equals(command, "ENCAP"))
		{
			// :<sid> ENCAP <server> <real-command>
			auto serverend = NextToken(line, cmdend);
			auto actualcmdend = NextToken(line, serverend);
			if (actualcmdend != std::string::npos)
			{
				std::string realcommand(line, serverend + 1, actualcmdend - serverend - 1);
				if (irc::equals(realcommand, "SASL"))
				{
					auto suidend = NextToken(line, actualcmdend);
					auto tuidend = NextToken(line, suidend);
					auto flagend = NextToken(line, tuidend);
					if (flagend != std::string::npos)
					{
						std::string flag(line, tuidend + 1, flagend - tuidend - 1);
						if (irc::equals(flag, "S"))
						{
							// SASL <uid> <uid> S <mech> <fp> [<fp>]+
							auto mechend = NextToken(line, flagend);
							auto fpend = NextToken(line, mechend);
							if (fpend != std::string::npos)
								line.erase(fpend); // Multiple fingerprints in ssl_cert was introduced in PROTO_INSPIRCD_4; drop it.
						}
					}
				}
			}
		}
		else if (irc::equals(command, "FHOST") || irc::equals(command, "FIDENT"))
		{
			// FIDENT/FHOST has two parameters in v4; drop the real username/hostname.
			// :<sid> FIDENT|FHOST <display|*> <real|*>
			size_t displayend = NextToken(line, cmdend);
			if (displayend != std::string::npos)
			{
				if ((displayend - cmdend) == 2 && line[displayend - 1] == '*')
					return; // FIDENT/FHOST is only changing the real username/hostname; drop.

				// Trim the rest of the line.
				line.erase(displayend);
			}
		}
		else if (irc::equals(command, "LMODE"))
		{
			// LMODE is new in v4; downgrade to FMODE.

			// :<sid>  LMODE <chan> <chants> <modechr> [<mask> <setts> <setter>]+
			auto chanend = NextToken(line, cmdend);
			auto chantsend = NextToken(line, chanend);
			auto modechrend = NextToken(line, chantsend);

			if (modechrend != std::string::npos)
			{
				// This should only be one character but its best to be sure...
				auto modechr = line.substr(chantsend + 1, modechrend - chantsend - 1);
				auto prevend = modechrend;

				// Build a new mode and parameter list.
				std::string modelist = "+";
				std::string paramlist;
				while (prevend != std::string::npos)
				{
					// We drop the setts and setter for the old protocol.
					auto maskend = NextToken(line, prevend);
					auto settsend = NextToken(line, maskend);
					if (settsend != std::string::npos)
					{
						modelist.append(modechr);
						paramlist.append(" ").append(line.substr(prevend + 1, maskend - prevend - 1));
					}
					prevend = NextToken(line, settsend);
				}

				// Replace the mode tuples with a list of mode values.
				line.replace(modechrend, line.length() - modechrend, paramlist);

				// Replace the mode character with a list of +modes
				line.replace(chantsend + 1, modechrend - chantsend - 1, modelist);

				// Replace LMODE with FMODE.
				line.replace(cmdstart, 5, "FMODE");
			}
		}
		else if (irc::equals(command, "METADATA"))
		{
			// :<sid> METADATA <uuid|chan|*|@> <name> :<value>
			size_t targetend = NextToken(line, cmdend);
			size_t nameend = NextToken(line, targetend);
			size_t flagend = NextToken(line, nameend);
			if (flagend != std::string::npos)
			{
				std::string extname(line, targetend + 1, nameend - targetend - 1);
				if (irc::equals(extname, "ssl_cert"))
				{
					// Check we have the "e" flag (no error).
					if (line.find('e', nameend + 1) < flagend)
					{
						size_t fpend = NextToken(line, flagend);
						if (fpend != std::string::npos)
						{
							size_t commapos = line.find(',', flagend + 1);
							if (commapos < fpend)
							{
								// Multiple fingerprints in ssl_cert was introduced in PROTO_INSPIRCD_4; drop it.
								line.erase(commapos, fpend - commapos);
							}

						}
					}
				}
			}
		}
		else if (irc::equals(command, "SINFO"))
		{
			// :<sid> SINFO <key> :<value>
			auto keyend = NextToken(line, cmdend);
			if (keyend != std::string::npos && sidstart != std::string::npos)
			{
				auto skey = line.substr(cmdend + 1, keyend - cmdend - 1);
				if (irc::equals(skey, "customversion"))
					return;
				else if (irc::equals(skey, "rawbranch"))
				{
					// InspIRCd-4. testnet.inspircd.org :Test
					auto* sid = Utils->FindServerID(line.substr(sidstart, cmdstart - sidstart - 1));
					if (sid)
					{
						line.replace(cmdend + 1, keyend - cmdend - 1, "version");
						line.append(INSP_FORMAT(". {} :{}", sid->GetPublicName(), sid->GetDesc()));
					}
				}
				else if (irc::equals(skey, "rawversion"))
				{
					// InspIRCd-4.0.0-a10. sadie.testnet.inspircd.org :[597] Test
					auto* sid = Utils->FindServerID(line.substr(sidstart, cmdstart - sidstart - 1));
					if (sid)
					{
						line.replace(cmdend + 1, keyend - cmdend - 1, "fullversion");
						line.append(INSP_FORMAT(". {} :[{}] {}", sid->GetName(), sid->GetId(), sid->GetDesc()));
					}
				}
			}
		}
		else if (irc::equals(command, "SQUERY"))
		{
			// SQUERY was introduced in PROTO_INSPIRCD_4; convert to PRIVMSG.
			line.replace(cmdstart, 6, "PRIVMSG");
		}
		else if (irc::equals(command, "UID"))
		{
			// :<sid> UID <uuid> <nickchanged> <nick> <host> <dhost> <user> <duser> <ip.string> <signon> <modes> [<modepara>] :<real>
			//                                                       ^^^^^^ New in 1206
			size_t uuidend = NextToken(line, cmdend);
			size_t nickchangedend = NextToken(line, uuidend);
			size_t nickend = NextToken(line, nickchangedend);
			size_t hostend = NextToken(line, nickend);
			size_t dhostend = NextToken(line, hostend);
			size_t userend = NextToken(line, dhostend);
			if (userend != std::string::npos)
				line.erase(dhostend, userend - dhostend);
		}
	}

	WriteLineInternal(line);
}

bool TreeSocket::PreProcessOldProtocolMessage(User*& who, std::string& cmd, CommandBase::Params& params)
{
	if (irc::equals(cmd, "FHOST") || irc::equals(cmd, "FIDENT"))
	{
		if (params.size() < 2)
			params.push_back("*");
	}
	else if (irc::equals(cmd, "SVSJOIN") || irc::equals(cmd, "SVSNICK") || irc::equals(cmd, "SVSPART"))
	{
		if (params.empty())
			return false; // Malformed.

		auto* target = ServerInstance->Users.FindUUID(params[0]);
		if (!target)
			return false; // User gone.

		params.insert(params.begin(), { target->uuid.substr(0, 3), cmd });
		cmd = "ENCAP";
	}
	else if (irc::equals(cmd, "UID"))
	{
		if (params.size() < 6)
			return false; // Malformed.

		// :<sid> UID <uuid> <nickchanged> <nick> <host> <dhost> <user> <duser> <ip.string> <signon> <modes> [<modepara>] :<real>
		//                                                       ^^^^^^ New in 1206
		params.insert(params.begin() + 5, params[5]);
	}
	return true;
}
