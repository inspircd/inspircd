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
#include "threadengines/threadengine_win32.h"
#include <pthread.h>

pthread_mutex_t MyMutex = PTHREAD_MUTEX_INITIALIZER;

Win32ThreadEngine::Win32ThreadEngine(InspIRCd* Instance) : ThreadEngine(Instance)
{
}

void Win32ThreadEngine::Create(Thread* thread_to_init)
{
	pthread_attr_t attribs;
	pthread_attr_init(&attribs);
	pthread_attr_setdetachstate(&attribs, PTHREAD_CREATE_JOINABLE);
	pthread_t* MyPThread = new pthread_t;

	if (pthread_create(MyPThread, &attribs, Win32ThreadEngine::Entry, (void*)this) != 0)
	{
		delete MyPThread;
		throw CoreException("Unable to create new Win32ThreadEngine: " + std::string(strerror(errno)));
	}

	pthread_attr_destroy(&attribs);

	NewThread = thread_to_init;
	NewThread->Creator = this;
	NewThread->Extend("pthread", MyPThread);
}

Win32ThreadEngine::~Win32ThreadEngine()
{
}

void Win32ThreadEngine::Run()
{
	NewThread->Run();
}

bool Win32ThreadEngine::Mutex(bool enable)
{
	if (enable)
		pthread_mutex_lock(&MyMutex);
	else
		pthread_mutex_unlock(&MyMutex);

	return false;
}

void* Win32ThreadEngine::Entry(void* parameter)
{
	ThreadEngine * pt = (ThreadEngine*)parameter;
	pt->Run();
	return NULL;
}

void Win32ThreadEngine::FreeThread(Thread* thread)
{
	pthread_t* pthread = NULL;
	if (thread->GetExt("pthread", pthread))
	{
		thread->SetExitFlag();
		int rc;
		void* status;
		rc = pthread_join(*pthread, &status);
		delete pthread;
	}
}

