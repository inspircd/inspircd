/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
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


#pragma once

class Module;

/** Timer class for one-second resolution timers
 * Timer provides a facility which allows module
 * developers to create one-shot timers. The timer
 * can be made to trigger at any time up to a one-second
 * resolution. To use Timer, inherit a class from
 * Timer, then insert your inherited class into the
 * queue using Server::AddTimer(). The Tick() method of
 * your object (which you have to override) will be called
 * at the given time.
 */
class CoreExport Timer
{
	/** The triggering time
	 */
	time_t trigger;

	/** Number of seconds between triggers
	 */
	unsigned int secs;

	/** True if this is a repeating timer
	 */
	bool repeat;

 public:
	/** Default constructor, initializes the triggering time
	 * @param secs_from_now The number of seconds from now to trigger the timer
	 * @param repeating Repeat this timer every secs_from_now seconds if set to true
	 */
	Timer(unsigned int secs_from_now, bool repeating = false);

	/** Default destructor, removes the timer from the timer manager
	 */
	virtual ~Timer();

	/** Retrieve the current triggering time
	 */
	time_t GetTrigger() const
	{
		return trigger;
	}

	/** Sets the trigger timeout to a new value
	 * This does not update the bookkeeping in TimerManager, use SetInterval()
	 * to change the interval between ticks while keeping TimerManager updated
	 */
	void SetTrigger(time_t nexttrigger)
	{
		trigger = nexttrigger;
	}

	/** Sets the interval between two ticks.
	 */
	void SetInterval(time_t interval);

	/** Called when the timer ticks.
	 * You should override this method with some useful code to
	 * handle the tick event.
	 * @param TIME The current time.
	 * @return True if the Timer object is still valid, false if it was destructed.
	 */
	virtual bool Tick(time_t TIME) = 0;

	/** Returns true if this timer is set to repeat
	 */
	bool GetRepeat() const
	{
		return repeat;
	}

	/** Returns the interval (number of seconds between ticks)
	 * of this timer object.
	 */
	unsigned int GetInterval() const
	{
		return secs;
	}

	/** Cancels the repeat state of a repeating timer.
	 * If you call this method, then the next time your
	 * timer ticks, it will be removed immediately after.
	 */
	void CancelRepeat()
	{
		repeat = false;
	}
};

/** This class manages sets of Timers, and triggers them at their defined times.
 * This will ensure timers are not missed, as well as removing timers that have
 * expired and allowing the addition of new ones.
 */
class CoreExport TimerManager
{
	typedef std::multimap<time_t, Timer*> TimerMap;

	/** A list of all pending timers
	 */
	TimerMap Timers;

 public:
	/** Tick all pending Timers
	 * @param TIME the current system time
	 */
	void TickTimers(time_t TIME);

	/** Add an Timer
	 * @param T an Timer derived class to add
	 */
	void AddTimer(Timer *T);

	/** Remove a Timer
	 * @param T an Timer derived class to remove
	 */
	void DelTimer(Timer* T);
};
