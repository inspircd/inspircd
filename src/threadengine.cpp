/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
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
