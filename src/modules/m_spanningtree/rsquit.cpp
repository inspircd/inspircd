/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rsquit.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rsquit.h */

cmd_rsquit::cmd_rsquit (InspIRCd* Instance, Module* Callback, SpanningTreeUtilities* Util) : command_t(Instance, "RSQUIT", 'o', 1), Creator(Callback), Utils(Util)
{
	this->source = "m_spanningtree.so";
	syntax = "<remote-server-mask> <target-server-mask>";
}

CmdResult cmd_rsquit::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (IS_LOCAL(user))
	{
		if (!Utils->FindServerMask(parameters[0]))
		{
			user->WriteServ("NOTICE %s :*** RSQUIT: Server \002%s\002 isn't connected to the network!", user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		user->WriteServ("NOTICE %s :*** RSQUIT: Sending remote squit to \002%s\002 to squit server \002%s\002.",user->nick,parameters[0],parameters[1]);
	}

	if (ServerInstance->MatchText(ServerInstance->Config->ServerName,parameters[0]))
	{
		ServerInstance->SNO->WriteToSnoMask('l',"Remote SQUIT from %s matching \002%s\002, squitting server \002%s\002",user->nick,parameters[0],parameters[1]);
		const char* para[1];
		para[0] = parameters[1];
		std::string original_command = std::string("SQUIT ") + parameters[1];
		Creator->OnPreCommand("SQUIT", para, 1, user, true, original_command);
	}

	return CMD_SUCCESS;
}
