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


#ifndef M_SPANNINGTREE_CACHETIMER_H
#define M_SPANNINGTREE_CACHETIMER_H

#include "timer.h"

class ModuleSpanningTree;
class SpanningTreeUtilities;

/** Create a timer which recurs every second, we inherit from Timer.
 * Timer is only one-shot however, so at the end of each Tick() we simply
 * insert another of ourselves into the pending queue :)
 */
class CacheRefreshTimer : public Timer
{
 private:
	SpanningTreeUtilities *Utils;
 public:
	CacheRefreshTimer(SpanningTreeUtilities* Util);
	virtual void Tick(time_t TIME);
};

#endif
