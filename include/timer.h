/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_TIMER_H
#define INSPIRCD_TIMER_H

/** Timer class for one-second resolution timers
 * InspTimer provides a facility which allows module
 * developers to create one-shot timers. The timer
 * can be made to trigger at any time up to a one-second
 * resolution. To use InspTimer, inherit a class from
 * InspTimer, then insert your inherited class into the
 * queue using Server::AddTimer(). The Tick() method of
 * your object (which you should override) will be called
 * at the given time.
 */
class InspTimer : public Extensible
{
 private:
	/** The triggering time
	 */
	time_t trigger;
 public:
	/** Default constructor, initializes the triggering time
	 */
	InspTimer(long secs_from_now,time_t now)
	{
		trigger = now + secs_from_now;
	}
	/** Default destructor, does nothing.
	 */
	virtual ~InspTimer() { }
	/** Retrieve the current triggering time
	 */
	virtual time_t GetTimer()
	{
		return trigger;
	}
	/** Called when the timer ticks.
	 */
	virtual void Tick(time_t TIME) = 0;
};

void TickTimers(time_t TIME);
void AddTimer(InspTimer* T);
void TickMissedTimers(time_t TIME);

#endif
