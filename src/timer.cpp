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

#include "inspircd.h"
#include "timer.h"

TimerManager::TimerManager(InspIRCd* Instance) : CantDeleteHere(false), ServerInstance(Instance)
{
}

void TimerManager::TickTimers(time_t TIME)
{
	this->CantDeleteHere = true;
	timerlist::iterator found = Timers.find(TIME);

	if (found != Timers.end())
	{
		timergroup* x = found->second;
		/* There are pending timers to trigger.
		 * WARNING: Timers may delete themselves from within
		 * their own Tick methods! see the comment below in
		 * the DelTimer method.
		 */
		for (timergroup::iterator y = x->begin(); y != x->end(); y++)
		{
			InspTimer* n = *y;
			n->Tick(TIME);
			if (n->GetRepeat())
			{
				AddTimer(n, n->GetSecs());
			}
			else
			{
				DELETE(n);
			}
		}

		Timers.erase(found);
		DELETE(x);
	}

	this->CantDeleteHere = false;
}

void TimerManager::DelTimer(InspTimer* T)
{
	if (this->CantDeleteHere)
	{
		/* If a developer tries to delete a timer from within its own Tick method,
		 * then chances are this is just going to totally fuck over the timergroup
		 * and timerlist iterators and cause a crash. Thanks to peavey and Bricker
		 * for noticing this bug.
		 * If we're within the tick loop when the DelTimer is called (signified
		 * by the var 'CantDeleteHere') then we simply return for non-repeating
		 * timers, and cancel the repeat on repeating timers. We can do this because
		 * we know that the timer tick loop will safely delete the timer for us
		 * anyway and therefore we avoid stack corruption.
		 */
		if (T->GetRepeat())
			T->CancelRepeat();
		else
			return;
	}

	timerlist::iterator found = Timers.find(T->GetTimer());

	if (found != Timers.end())
	{
		timergroup* x = found->second;
		for (timergroup::iterator y = x->begin(); y != x->end(); y++)
		{
			InspTimer* n = *y;
			if (n == T)
			{
				DELETE(n);
				x->erase(y);
				if (!x->size())
				{
					Timers.erase(found);
					DELETE(x);
				}
				return;
			}
		}
	}
}

/** Because some muppets may do odd things, and their ircd may lock up due
 * to crappy 3rd party modules, or they may change their system time a bit,
 * this accounts for shifts of up to 120 secs by looking behind for missed
 * timers and executing them. This is only executed once every 5 secs.
 * If you move your clock BACK, and your timers move further ahead as a result,
 * then tough titty you'll just have to wait.
 */
void TimerManager::TickMissedTimers(time_t TIME)
{
	for (time_t n = TIME-1; n > TIME-120; n--)
		this->TickTimers(TIME);
}

void TimerManager::AddTimer(InspTimer* T, long secs_from_now)
{
	timergroup* x = NULL;

	int time_to_trigger = 0;
	if (!secs_from_now)
		time_to_trigger = T->GetTimer();
	else
		time_to_trigger = secs_from_now + ServerInstance->Time();

	timerlist::iterator found = Timers.find(time_to_trigger);

	if (found != Timers.end())
	{
		x = found->second;
	}
	else
	{
		x = new timergroup;
		Timers[time_to_trigger] = x;
	}

	x->push_back(T);
}

