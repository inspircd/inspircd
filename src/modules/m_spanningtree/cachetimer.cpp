/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

#include "cachetimer.h"
#include "utils.h"

/* $ModDep: m_spanningtree/cachetimer.h m_spanningtree/utils.h */

CacheRefreshTimer::CacheRefreshTimer(SpanningTreeUtilities* Util) : Timer(3600, ServerInstance->Time(), true), Utils(Util)
{
}

bool CacheRefreshTimer::Tick(time_t TIME)
{
	Utils->RefreshIPCache();
	return true;
}

