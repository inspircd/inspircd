/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_TIMER_H
#define INSPIRCD_TIMER_H

//class InspIRCd;

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
class CoreExport Timer : public Extensible
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
class CoreExport TimerManager : public Extensible
{
 protected:
	/** A list of all pending timers
	 */
	std::vector<Timer *> Timers;

	/** Creating server instance
	 */
	InspIRCd* ServerInstance;
 public:
	/** Constructor
	 */
	TimerManager(InspIRCd* Instance);

	/** Tick all pending Timers
	 * @param TIME the current system time
	 */
	void TickTimers(time_t TIME);

	/** Add an Timer
	 * @param T an Timer derived class to add
	 * @param secs_from_now You may set this to the number of seconds
	 * from the current time when the timer will tick, or you may just
	 * leave this unset and the values set by the Timers constructor
	 * will be used. This is used internally for re-triggering repeating
	 * timers.
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

