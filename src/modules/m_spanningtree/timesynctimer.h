#ifndef __TIMESYNC_H__
#define __TIMESYNC_H__

#include "timer.h"

class ModuleSpanningTree;
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

#endif
