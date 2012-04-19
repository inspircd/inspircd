/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
