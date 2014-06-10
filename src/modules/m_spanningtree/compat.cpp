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
#include "treeserver.h"

static std::string newline("\n");

void TreeSocket::WriteLine(const std::string& original_line)
{
	if (LinkState == CONNECTED)
	{
		if (original_line.c_str()[0] != ':')
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Sending line without server prefix!");
			WriteLine(":" + ServerInstance->Config->GetSID() + " " + original_line);
			return;
		}
		if (proto_version != ProtocolVersion)
		{
			std::string line = original_line;
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
				else if (command == "METADATA")
				{
					// Drop TS for channel METADATA, translate METADATA operquit into an OPERQUIT command
					// :sid METADATA #target TS extname ...
					//     A        B       C  D
					if (b == std::string::npos)
						return;

					std::string::size_type c = line.find(' ', b + 1);
					if (c == std::string::npos)
						return;

					std::string::size_type d = line.find(' ', c + 1);
					if (d == std::string::npos)
						return;

					if (line[b + 1] == '#')
					{
						// We're sending channel metadata
						line.erase(c, d-c);
					}
					else if (!line.compare(c, d-c, " operquit", 9))
					{
						// ":22D METADATA 22DAAAAAX operquit :message" -> ":22DAAAAAX OPERQUIT :message"
						line = ":" + line.substr(b+1, c-b) + "OPERQUIT" + line.substr(d);
					}
				}
				else if (command == "FTOPIC")
				{
					// Drop channel TS for FTOPIC
					// :sid FTOPIC #target TS TopicTS setter :newtopic
					//     A      B       C  D       E      F
					// :uid FTOPIC #target TS TopicTS :newtopic
					//     A      B       C  D       E
					if (b == std::string::npos)
						return;

					std::string::size_type c = line.find(' ', b + 1);
					if (c == std::string::npos)
						return;

					std::string::size_type d = line.find(' ', c + 1);
					if (d == std::string::npos)
						return;

					std::string::size_type e = line.find(' ', d + 1);
					if (line[e+1] == ':')
					{
						line.erase(c, e-c);
						line.erase(a+1, 1);
					}
					else
						line.erase(c, d-c);
				}
				else if ((command == "PING") || (command == "PONG"))
				{
					// :22D PING 20D
					if (line.length() < 13)
						return;

					// Insert the source SID (and a space) between the command and the first parameter
					line.insert(10, line.substr(1, 4));
				}
				else if (command == "OPERTYPE")
				{
					std::string::size_type colon = line.find(':', b);
					if (colon != std::string::npos)
					{
						for (std::string::iterator i = line.begin()+colon; i != line.end(); ++i)
						{
							if (*i == ' ')
								*i = '_';
						}
						line.erase(colon, 1);
					}
				}
			}
			ServerInstance->Logs->Log(MODNAME, LOG_RAWIO, "S[%d] O %s", this->GetFd(), line.c_str());
			this->WriteData(line);
			this->WriteData(newline);
			return;
		}
	}

	ServerInstance->Logs->Log(MODNAME, LOG_RAWIO, "S[%d] O %s", this->GetFd(), original_line.c_str());
	this->WriteData(original_line);
	this->WriteData(newline);
}

namespace
{
	bool InsertCurrentChannelTS(std::vector<std::string>& params, unsigned int chanindex = 0, unsigned int pos = 1)
	{
		Channel* chan = ServerInstance->FindChan(params[chanindex]);
		if (!chan)
			return false;

		// Insert the current TS of the channel after the pos-th parameter
		params.insert(params.begin()+pos, ConvToStr(chan->age));
		return true;
	}
}

bool TreeSocket::PreProcessOldProtocolMessage(User*& who, std::string& cmd, std::vector<std::string>& params)
{
	if ((cmd == "METADATA") && (params.size() >= 3) && (params[0][0] == '#'))
	{
		// :20D METADATA #channel extname :extdata
		return InsertCurrentChannelTS(params);
	}
	else if ((cmd == "FTOPIC") && (params.size() >= 4))
	{
		// :20D FTOPIC #channel 100 Attila :topic text
		return InsertCurrentChannelTS(params);
	}
	else if ((cmd == "PING") || (cmd == "PONG"))
	{
		if (params.size() == 1)
		{
			// If it's a PING with 1 parameter, reply with a PONG now, if it's a PONG with 1 parameter (weird), do nothing
			if (cmd[1] == 'I')
				this->WriteData(":" + ServerInstance->Config->GetSID() + " PONG " + params[0] + newline);

			// Don't process this message further
			return false;
		}

		// :20D PING 20D 22D
		// :20D PONG 20D 22D
		// Drop the first parameter
		params.erase(params.begin());

		// If the target is a server name, translate it to a SID
		if (!InspIRCd::IsSID(params[0]))
		{
			TreeServer* server = Utils->FindServer(params[0]);
			if (!server)
			{
				// We've no idea what this is, log and stop processing
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Received a " + cmd + " with an unknown target: \"" + params[0] + "\", command dropped");
				return false;
			}

			params[0] = server->GetID();
		}
	}
	else if ((cmd == "GLINE") || (cmd == "KLINE") || (cmd == "ELINE") || (cmd == "ZLINE") || (cmd == "QLINE"))
	{
		// Fix undocumented protocol usage: translate GLINE, ZLINE, etc. into ADDLINE or DELLINE
		if ((params.size() != 1) && (params.size() != 3))
			return false;

		parameterlist p;
		p.push_back(cmd.substr(0, 1));
		p.push_back(params[0]);

		if (params.size() == 3)
		{
			cmd = "ADDLINE";
			p.push_back(who->nick);
			p.push_back(ConvToStr(ServerInstance->Time()));
			p.push_back(ConvToStr(InspIRCd::Duration(params[1])));
			p.push_back(params[2]);
		}
		else
			cmd = "DELLINE";

		params.swap(p);
	}
	else if (cmd == "SVSMODE")
	{
		cmd = "MODE";
	}
	else if (cmd == "OPERQUIT")
	{
		// Translate OPERQUIT into METADATA
		if (params.empty())
			return false;

		cmd = "METADATA";
		params.insert(params.begin(), who->uuid);
		params.insert(params.begin()+1, "operquit");
		who = MyRoot->ServerUser;
	}
	else if ((cmd == "TOPIC") && (params.size() >= 2))
	{
		// :20DAAAAAC TOPIC #chan :new topic
		cmd = "FTOPIC";
		if (!InsertCurrentChannelTS(params))
			return false;

		params.insert(params.begin()+2, ConvToStr(ServerInstance->Time()));
	}
	else if (cmd == "MODENOTICE")
	{
		// MODENOTICE is always supported by 2.0 but it's optional in 2.2.
		params.insert(params.begin(), "*");
		params.insert(params.begin()+1, cmd);
		cmd = "ENCAP";
	}
	else if (cmd == "RULES")
	{
		return false;
	}

	return true; // Passthru
}
