/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

/*********        DEFAULTS       **********/
/* $ExtraSources: threadengines/threadengine_pthread.cpp */
/* $ExtraObjects: threadengine_pthread.o */

#include "inspircd.h"
#include "threadengine.h"

void Thread::SetExitFlag()
{
	ExitFlag = true;
}

void Thread::join()
{
		state->FreeThread(this);
		delete state;
		state = 0;
}

/** If this thread has a Creator set, call it to
 * free the thread
 */
Thread::~Thread()
{
}
