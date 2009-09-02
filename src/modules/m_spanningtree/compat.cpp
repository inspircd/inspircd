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

void TreeSocket::WriteLine(std::string line)
{
	if (line[0] != ':' && LinkState == CONNECTED)
	{
		ServerInstance->Logs->Log("m_spanningtree", DEFAULT, "Sending line without server prefix!");
		line = ":" + ServerInstance->Config->GetSID() + " " + line;
	}
	if (proto_version != ProtocolVersion)
	{
		std::string::size_type a = line.find(' ');
		std::string::size_type b = line.find(' ', a);
		std::string command = line.substr(a,b);
		// now try to find a translation entry
		// TODO a more efficient lookup method will be needed later
		if (proto_version < 1202 && command == "FIDENT")
		{
			// a more aggressive method would be to translate to CHGIDENT
			ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Dropping FIDENT to 1201-protocol server");
			return;
		}
	}

	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "S[%d] O %s", this->GetFd(), line.c_str());
	line.append("\r\n");
	this->Write(line);
}



