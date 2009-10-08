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

#include "cachetimer.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/cachetimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

CacheRefreshTimer::CacheRefreshTimer(SpanningTreeUtilities *Util) : Timer(3600, ServerInstance->Time(), true), Utils(Util)
{
}

void CacheRefreshTimer::Tick(time_t TIME)
{
	Utils->RefreshIPCache();
}

