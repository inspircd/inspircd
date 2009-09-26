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

#ifndef __TIMESYNC_H__
#define __TIMESYNC_H__

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
