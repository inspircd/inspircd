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
#include "socket.h"
#include "xline.h"
#include "../transport.h"

#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "rconnect.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rconnect.h */

CommandRConnect::CommandRConnect (InspIRCd* Instance, Module* Creator, SpanningTreeUtilities* Util)
	: Command(Instance, Creator, "RCONNECT", "o", 2), Utils(Util)
{
	syntax = "<remote-server-mask> <target-server-mask>";
}

CmdResult CommandRConnect::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (IS_LOCAL(user))
	{
		if (!Utils->FindServerMask(parameters[0]))
		{
			user->WriteServ("NOTICE %s :*** RCONNECT: Server \002%s\002 isn't connected to the network!", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		user->WriteServ("NOTICE %s :*** RCONNECT: Sending remote connect to \002%s\002 to connect server \002%s\002.",user->nick.c_str(),parameters[0].c_str(),parameters[1].c_str());
	}

	/* Is this aimed at our server? */
	if (InspIRCd::Match(ServerInstance->Config->ServerName,parameters[0]))
	{
		/* Yes, initiate the given connect */
		ServerInstance->SNO->WriteToSnoMask('l',"Remote CONNECT from %s matching \002%s\002, connecting server \002%s\002",user->nick.c_str(),parameters[0].c_str(),parameters[1].c_str());
		std::vector<std::string> para;
		para.push_back(parameters[1]);
		std::string cmd("CONNECT");
		std::string original_command = cmd + " " + parameters[1];
		creator->OnPreCommand(cmd, para, user, true, original_command);
	}
	return CMD_SUCCESS;
}

