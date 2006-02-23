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

using namespace std;

#include "inspircd_config.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "timer.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspstring.h"
#include "helperfuncs.h"

extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern time_t TIME;

typedef std::vector<InspTimer*> timergroup;
typedef std::map<time_t, timergroup*> timerlist;
timerlist Timers;

void TickTimers(time_t TIME)
{
	timerlist::iterator found = Timers.find(TIME);

	if (found != Timers.end())
	{
		log("timer.cpp: There are timers to trigger");
		timerlist* x = found->second;
		/*
		 * There are pending timers to trigger
		 */
		for (timerlist::iterator y = x.begin(); y != x.end(); y++)
		{
			log("timer.cpp: Triggering a timer");
			InspTimer* n = (InspTimer*)*y;
			n->Tick(TIME);
			log("timer.cpp: TICK!");
			delete n;
		}

		log("timer.cpp: Done triggering timers, tidying up");
		Timers.erase(found);
		delete x;
	}
}

void AddTimer(InspTimer* T)
{
	log("timer.cpp: Adding timer");
	
	timergroup* x = NULL;

	timerlist::iterator found = Timers.find(T->GetTimer());
	
	if (found != Timers.end())
	{
		log("timer.cpp: Add timer to existing group");
		x = found->second;
	}
	else
	{
		log("timer.cpp: Add timer to new group");
		x = new timergroup;
		Timers[T->GetTimer()] = x;
	}

	x->push_back(T);
}
