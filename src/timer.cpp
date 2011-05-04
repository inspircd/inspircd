/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "timer.h"

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
	for(std::multimap<time_t, Timer *>::iterator i = Timers.begin(); i != Timers.end(); i++)
		delete i->second;
}

void TimerManager::TickTimers(time_t TIME)
{
	while (!Timers.empty())
	{
		std::multimap<time_t, Timer*>::iterator i = Timers.begin();
		Timer* t = i->second;
		if (t->GetTimer() > TIME)
			return;
		Timers.erase(i);

		t->Tick(TIME);

		if (t->GetRepeat())
		{
			t->SetTimer(TIME + t->GetSecs());
			AddTimer(t);
		}
		else
			delete t;
	}
}

void TimerManager::DelTimer(Timer* T)
{
	std::multimap<time_t, Timer*>::iterator i = Timers.find(T->GetTimer());
	while (1)
	{
		if (i == Timers.end())
			return;
		if (i->second == T)
			break;
		if (i->second->GetTimer() != T->GetTimer())
			return;
		i++;
	}

	Timers.erase(i);
	delete T;
}

void TimerManager::AddTimer(Timer* T)
{
	Timers.insert(std::make_pair(T->GetTimer(), T));
}
