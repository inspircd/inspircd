/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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


/* $Core */

#include "inspircd.h"
#include "timer.h"

TimerManager::TimerManager()
{
}

TimerManager::~TimerManager()
{
	for(std::vector<Timer *>::iterator i = Timers.begin(); i != Timers.end(); i++)
		delete *i;
}

void TimerManager::TickTimers(time_t TIME)
{
	while ((Timers.size()) && (TIME > (*Timers.begin())->GetTimer()))
	{
		std::vector<Timer *>::iterator i = Timers.begin();
		Timer *t = (*i);

		// Probable fix: move vector manipulation to *before* we modify the vector.
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
	std::sort(Timers.begin(), Timers.end(), TimerManager::TimerComparison);
}

bool TimerManager::TimerComparison( Timer *one, Timer *two)
{
	return (one->GetTimer()) < (two->GetTimer());
}
