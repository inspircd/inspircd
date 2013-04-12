/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "treesocket.h"

static std::string newline("\n");

void TreeSocket::WriteLine(std::string line)
{
	if (LinkState == CONNECTED)
	{
		if (line[0] != ':')
		{
			ServerInstance->Logs->Log("m_spanningtree", LOG_DEFAULT, "Sending line without server prefix!");
			line = ":" + ServerInstance->Config->GetSID() + " " + line;
		}
		if (proto_version != ProtocolVersion)
		{
			std::string::size_type a = line.find(' ');
			std::string::size_type b = line.find(' ', a + 1);
			std::string command = line.substr(a + 1, b-a-1);
			// now try to find a translation entry
			// TODO a more efficient lookup method will be needed later
			if (proto_version < 1205)
			{
				if (command == "IJOIN")
				{
					// Convert
					// :<uid> IJOIN <chan> [<ts> [<flags>]]
					// to
					// :<sid> FJOIN <chan> <ts> + [<flags>],<uuid>
					std::string::size_type c = line.find(' ', b + 1);
					if (c == std::string::npos)
					{
						// No TS or modes in the command
						// :22DAAAAAB IJOIN #chan
						const std::string channame = line.substr(b+1, c-b-1);
						Channel* chan = ServerInstance->FindChan(channame);
						if (!chan)
							return;

						line.push_back(' ');
						line.append(ConvToStr(chan->age));
						line.append(" + ,");
					}
					else
					{
						std::string::size_type d = line.find(' ', c + 1);
						if (d == std::string::npos)
						{
							// TS present, no modes
							// :22DAAAAAC IJOIN #chan 12345
							line.append(" + ,");
						}
						else
						{
							// Both TS and modes are present
							// :22DAAAAAC IJOIN #chan 12345 ov
							std::string::size_type e = line.find(' ', d + 1);
							if (e != std::string::npos)
								line.erase(e);

							line.insert(d, " +");
							line.push_back(',');
						}
					}

					// Move the uuid to the end and replace the I with an F
					line.append(line.substr(1, 9));
					line.erase(4, 6);
					line[5] = 'F';
				}
				else if (command == "RESYNC")
					return;
			}
		}
	}

	ServerInstance->Logs->Log("m_spanningtree", LOG_RAWIO, "S[%d] O %s", this->GetFd(), line.c_str());
	this->WriteData(line);
	this->WriteData(newline);
}
