/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <vector>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include "typedefs.h"
#include "helperfuncs.h"
#include "timer.h"

typedef std::vector<InspTimer*> timergroup;
typedef std::map<time_t, timergroup*> timerlist;

timerlist Timers;

void TickTimers(time_t TIME)
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
			delete n;
		}

		Timers.erase(found);
		delete x;
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
void TickMissedTimers(time_t TIME)
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
				delete z;
			}

			Timers.erase(found);
			delete x;
		}
	}
}

void AddTimer(InspTimer* T)
{
	timergroup* x = NULL;

	timerlist::iterator found = Timers.find(T->GetTimer());
	
	if (found != Timers.end())
	{
		x = found->second;
	}
	else
	{
		x = new timergroup;
		Timers[T->GetTimer()] = x;
	}

	x->push_back(T);
}
