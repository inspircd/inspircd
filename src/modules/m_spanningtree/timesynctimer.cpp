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
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

TimeSyncTimer::TimeSyncTimer(InspIRCd *Inst, ModuleSpanningTree *Mod) : InspTimer(600, Inst->Time(), true), Instance(Inst), Module(Mod)
{
}

void TimeSyncTimer::Tick(time_t TIME)
{
	Module->BroadcastTimeSync();
}

CacheRefreshTimer::CacheRefreshTimer(InspIRCd *Inst, SpanningTreeUtilities *Util) : InspTimer(3600, Inst->Time(), true), Instance(Inst), Utils(Util)
{
}

void CacheRefreshTimer::Tick(time_t TIME)
{
	Utils->RefreshIPCache();
}

