/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
