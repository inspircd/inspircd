/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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
	time_t trigger = 0;

	/** Number of seconds between triggers
	 */
	unsigned long secs;

	/** True if this is a repeating timer
	 */
	bool repeat;

public:
	/** Default constructor, initializes the triggering time
	 * @param secs_from_now The number of seconds from now to trigger the timer
	 * @param repeating Repeat this timer every secs_from_now seconds if set to true
	 */
	Timer(unsigned long secs_from_now, bool repeating);

	/** Default destructor, removes the timer from the timer manager
	 */
	virtual ~Timer();

	/** Retrieves the time at which this timer will tick next. If the timer is not active then 0 will be returned. */
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
	void SetInterval(unsigned long interval, bool restart = true);

	/** Called when the timer ticks.
	 * You should override this method with some useful code to
	 * handle the tick event.
	 * @return True if the Timer object is still valid, false if it was destructed.
	 */
	virtual bool Tick() = 0;

	/** Returns true if this timer is set to repeat
	 */
	bool GetRepeat() const
	{
		return repeat;
	}

	/** Returns the interval (number of seconds between ticks)
	 * of this timer object.
	 */
	unsigned long GetInterval() const
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
class CoreExport TimerManager final
{
	typedef std::multimap<time_t, Timer*> TimerMap;

	/** A list of all pending timers
	 */
	TimerMap Timers;

public:
	/** Tick all pending Timers
	 */
	void TickTimers();

	/** Add an Timer
	 * @param T an Timer derived class to add
	 */
	void AddTimer(Timer* T);

	/** Remove a Timer
	 * @param T an Timer derived class to remove
	 */
	void DelTimer(Timer* T);
};
