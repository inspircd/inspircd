/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDtimer */

#include "inspircd.h"
#include "timer.h"

TimerManager::TimerManager(InspIRCd* Instance) : ServerInstance(Instance)
{
}

void TimerManager::TickTimers(time_t TIME)
{
	while ((Timers.size()) && (TIME > (*Timers.begin())->GetTimer()))
	{
		std::vector<Timer *>::iterator i = Timers.begin();
		Timer *t = (*i);

		t->Tick(TIME);
		if (t->GetRepeat())
		{
			t->SetTimer(TIME + t->GetSecs());
			AddTimer(t);
		}
		else
			delete t;

		Timers.erase(i);
	}
}

void TimerManager::DelTimer(Timer* T)
{
	std::vector<Timer *>::iterator i = std::find(Timers.begin(), Timers.end(), T);

	if (i != Timers.end())
	{
		delete (*i);
		Timers.erase(i);
	}
}

void TimerManager::AddTimer(Timer* T)
{
	Timers.push_back(T);
	sort(Timers.begin(), Timers.end(), TimerManager::TimerComparison);
}

bool TimerManager::TimerComparison( Timer *one, Timer *two)
{
	return (one->GetTimer()) < (two->GetTimer());
}


