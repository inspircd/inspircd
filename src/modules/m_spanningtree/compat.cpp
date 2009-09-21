/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "main.h"
#include "treesocket.h"

static const char* const forge_common_1201[] = {
	"m_chghost.so",
	"m_chgname.so",
	"m_remove.so",
	"m_sajoin.so",
	"m_sakick.so",
	"m_sanick.so",
	"m_sapart.so",
	"m_saquit.so",
	"m_setident.so",
};

void TreeSocket::CompatAddModules(std::vector<std::string>& modlist)
{
	if (proto_version < 1202)
	{
		// you MUST have chgident loaded in order to be able to translate FIDENT
		modlist.push_back("m_chgident.so");
		for(int i=0; i * sizeof(char*) < sizeof(forge_common_1201); i++)
		{
			if (ServerInstance->Modules->Find(forge_common_1201[i]))
				modlist.push_back(forge_common_1201[i]);
		}
	}
}

void TreeSocket::WriteLine(std::string line)
{
	if (LinkState == CONNECTED)
	{
		if (line[0] != ':')
		{
			ServerInstance->Logs->Log("m_spanningtree", DEFAULT, "Sending line without server prefix!");
			line = ":" + ServerInstance->Config->GetSID() + " " + line;
		}
		if (proto_version != ProtocolVersion)
		{
			std::string::size_type a = line.find(' ') + 1;
			std::string::size_type b = line.find(' ', a);
			std::string command = line.substr(a, b-a);
			// now try to find a translation entry
			// TODO a more efficient lookup method will be needed later
			if (proto_version < 1202 && command == "FIDENT")
			{
				ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Rewriting FIDENT for 1201-protocol server");
				line = ":" + ServerInstance->Config->GetSID() + " CHGIDENT " +  line.substr(1,a-2) + line.substr(b);
			}
			else if (proto_version < 1202 && command == "SAVE")
			{
				ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Rewriting SAVE for 1201-protocol server");
				std::string::size_type c = line.find(' ', b + 1);
				std::string uid = line.substr(b, c - b);
				line = ":" + ServerInstance->Config->GetSID() + " SVSNICK " + uid + line.substr(b);
			}
			else if (proto_version < 1202 && command == "AWAY")
			{
				if (b != std::string::npos)
				{
					std::string::size_type c = line.find(' ', b + 1);
					line.erase(b,c-b);
				}
			}
		}
	}

	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "S[%d] O %s", this->GetFd(), line.c_str());
	line.append("\r\n");
	this->WriteData(line);
}



