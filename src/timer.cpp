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

void TimerManager::TickTimers(time_t TIME)
{
	timerlist::iterator found = Timers.find(TIME);

	if (found != Timers.end())
	{
		timergroup* x = found->second;
		/*
		 * There are pending timers to trigger
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
}

void TimerManager::DelTimer(InspTimer* T)
{
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
					Timers.erase(found);
				return;
			}
		}
	}
}

/*
 * Because some muppets may do odd things, and their ircd may lock up due
 * to crappy 3rd party modules, or they may change their system time a bit,
 * this accounts for shifts of up to 120 secs by looking behind for missed
 * timers and executing them. This is only executed once every 5 secs.
 * If you move your clock BACK, and your timers move further ahead as a result,
 * then tough titty you'll just have to wait.
 */
void TimerManager::TickMissedTimers(time_t TIME)
{
	for (time_t n = TIME-1; n > TIME-120; n--)
	{
		timerlist::iterator found = Timers.find(n);
		if (found != Timers.end())
		{
			timergroup* x = found->second;
			for (timergroup::iterator y = x->begin(); y != x->end(); y++)
			{
				InspTimer* z = *y;
				z->Tick(TIME);
				if (z->GetRepeat())
				{
					AddTimer(z, z->GetSecs());
				}
				else
				{
					DELETE(z);
				}
			}

			Timers.erase(found);
			DELETE(x);
		}
	}
}

void TimerManager::AddTimer(InspTimer* T, long secs_from_now)
{
	timergroup* x = NULL;

	int time_to_trigger = 0;
	if (!secs_from_now)
		time_to_trigger = T->GetTimer();
	else
		time_to_trigger = secs_from_now + time(NULL);

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

