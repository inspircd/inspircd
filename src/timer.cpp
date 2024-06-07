/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

void Timer::SetInterval(unsigned long newinterval, bool restart)
{
	secs = newinterval;
	if (!restart)
		return;

	ServerInstance->Timers.DelTimer(this);
	SetTrigger(ServerInstance->Time() + newinterval);
	ServerInstance->Timers.AddTimer(this);
}

Timer::Timer(unsigned long secs_from_now, bool repeating)
	: secs(secs_from_now)
	, repeat(repeating)
{
}

Timer::~Timer()
{
	if (GetTrigger())
		ServerInstance->Timers.DelTimer(this);
}

void TimerManager::TickTimers()
{
	const time_t now = ServerInstance->Time();
	for (TimerMap::iterator i = Timers.begin(); i != Timers.end(); )
	{
		Timer* t = i->second;
		if (t->GetTrigger() > now)
			break;

		Timers.erase(i++);

		if (!t->Tick())
			continue;

		if (t->GetRepeat())
		{
			t->SetTrigger(now + t->GetInterval());
			AddTimer(t);
		}
	}
}

void TimerManager::DelTimer(Timer* t)
{
	std::pair<TimerMap::iterator, TimerMap::iterator> itpair = Timers.equal_range(t->GetTrigger());

	for (TimerMap::iterator i = itpair.first; i != itpair.second; ++i)
	{
		if (i->second == t)
		{
			t->SetTrigger(0);
			Timers.erase(i);
			break;
		}
	}
}

void TimerManager::AddTimer(Timer* t)
{
	time_t trigger = ServerInstance->Time() + t->GetInterval();
	t->SetTrigger(trigger);
	Timers.emplace(trigger, t);
}
