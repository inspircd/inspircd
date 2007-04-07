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

#ifndef __TIMESYNC_H__
#define __TIMESYNC_H__

#include "timer.h"

class ModuleSpanningTree;
class SpanningTreeUtilities;
class InspIRCd;

/** Create a timer which recurs every second, we inherit from InspTimer.
 * InspTimer is only one-shot however, so at the end of each Tick() we simply
 * insert another of ourselves into the pending queue :)
 */
class TimeSyncTimer : public InspTimer
{
 private:
	InspIRCd *Instance;
	ModuleSpanningTree *Module;
 public:
	TimeSyncTimer(InspIRCd *Instance, ModuleSpanningTree *Mod);
	virtual void Tick(time_t TIME);
};

class CacheRefreshTimer : public InspTimer
{
 private:
	InspIRCd *Instance;
	SpanningTreeUtilities *Utils;
 public:
	CacheRefreshTimer(InspIRCd *Instance, SpanningTreeUtilities* Util);
	virtual void Tick(time_t TIME);
};

#endif
