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


#ifndef INSPIRCD_TIMER_H
#define INSPIRCD_TIMER_H

/** Timer class for one-second resolution timers
 * Timer provides a facility which allows module
 * developers to create one-shot timers. The timer
 * can be made to trigger at any time up to a one-second
 * resolution. To use Timer, inherit a class from
 * Timer, then insert your inherited class into the
 * queue using Server::AddTimer(). The Tick() method of
 * your object (which you should override) will be called
 * at the given time.
 */
class CoreExport Timer
{
 private:
	/** The triggering time
	 */
	time_t trigger;
	/** Number of seconds between triggers
	 */
	long secs;
	/** True if this is a repeating timer
	 */
	bool repeat;
 public:
	/** Default constructor, initializes the triggering time
	 * @param secs_from_now The number of seconds from now to trigger the timer
	 * @param now The time now
	 * @param repeating Repeat this timer every secs_from_now seconds if set to true
	 */
	Timer(long secs_from_now, time_t now, bool repeating = false)
	{
		trigger = now + secs_from_now;
		secs = secs_from_now;
		repeat = repeating;
	}

	/** Default destructor, does nothing.
	 */
	virtual ~Timer() { }

	/** Retrieve the current triggering time
	 */
	virtual time_t GetTimer()
	{
		return trigger;
	}

	/** Sets the trigger timeout to a new value
	 */
	virtual void SetTimer(time_t t)
	{
		trigger = t;
	}

	/** Called when the timer ticks.
	 * You should override this method with some useful code to
	 * handle the tick event.
	 */
	virtual void Tick(time_t TIME) = 0;

	/** Returns true if this timer is set to repeat
	 */
	bool GetRepeat()
	{
		return repeat;
	}

	/** Returns the interval (number of seconds between ticks)
	 * of this timer object.
	 */
	long GetSecs()
	{
		return secs;
	}

	/** Cancels the repeat state of a repeating timer.
	 * If you call this method, then the next time your
	 * timer ticks, it will be removed immediately after.
	 * You should use this method call to remove a recurring
	 * timer if you wish to do so within the timer's Tick
	 * event, as calling TimerManager::DelTimer() from within
	 * the Timer::Tick() method is dangerous and may
	 * cause a segmentation fault. Calling CancelRepeat()
	 * is safe in this case.
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
 protected:
	/** A list of all pending timers
	 */
	std::vector<Timer *> Timers;

 public:
	/** Constructor
	 */
	TimerManager();
	~TimerManager();

	/** Tick all pending Timers
	 * @param TIME the current system time
	 */
	void TickTimers(time_t TIME);

	/** Add an Timer
	 * @param T an Timer derived class to add
	 */
	void AddTimer(Timer *T);

	/** Delete an Timer
	 * @param T an Timer derived class to delete
	 */
	void DelTimer(Timer* T);

	/** Compares two timers
	 */
	static bool TimerComparison( Timer *one,  Timer*two);
};

#endif

