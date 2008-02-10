/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDthreadengine */

/*********        DEFAULTS       **********/
/* $ExtraSources: socketengines/socketengine_pthread.cpp */
/* $ExtraObjects: socketengine_pthread.o */

/* $If: USE_WIN32 */
/* $ExtraSources: socketengines/socketengine_win32.cpp */
/* $ExtraObjects: socketengine_win32.o */
/* $EndIf */

#include "inspircd.h"
#include "threadengines/threadengine_pthread.h"
#include <pthread.h>

pthread_mutex_t MyMutex = PTHREAD_MUTEX_INITIALIZER;

PThreadEngine::PThreadEngine(InspIRCd* Instance) : ThreadEngine(Instance)
{
}

void PThreadEngine::Create(Thread* thread_to_init)
{
	pthread_attr_t attribs;
	pthread_attr_init(&attribs);
	if (pthread_create(&this->MyPThread,
				&attribs,
				PThreadEngine::Entry,
				(void*)this) != 0)
	{
		throw CoreException("Unable to create new PThreadEngine: " + std::string(strerror(errno)));
	}
	NewThread = thread_to_init;
}

PThreadEngine::~PThreadEngine()
{
	//pthread_kill(this->MyPThread, SIGKILL);
}

void PThreadEngine::Run()
{
	/* This is a factory function that will create a class of type 'Thread'. The class of type Thread just
	 * takes an InspIRCd* pointer and a ThreadEngine* pointer in its ctor (so it can easily use the Mutex
	 * methods etc) and runs asyncronously of the ircd. */
	NewThread->Run();
}

bool PThreadEngine::Mutex(bool enable)
{
	if (enable)
		pthread_mutex_lock(&MyMutex);
	else
		pthread_mutex_unlock(&MyMutex);

	return false;
}

void* PThreadEngine::Entry(void* parameter)
{
	ThreadEngine * pt = (ThreadEngine*)parameter;
	pt->Run();
	return NULL;
}

