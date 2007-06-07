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
		return (CmdResult)((ModuleSpanningTree*)Creator)->HandleSquit(parameters, pcnt, user, true);
	}
	return CMD_SUCCESS;
}
