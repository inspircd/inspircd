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
	pthread_attr_setdetachstate(&attribs, PTHREAD_CREATE_JOINABLE);
	pthread_t* MyPThread = new pthread_t;

	Mutex(true);

	if (pthread_create(MyPThread, &attribs, PThreadEngine::Entry, (void*)this) != 0)
	{
		delete MyPThread;
		Mutex(false);
		throw CoreException("Unable to create new PThreadEngine: " + std::string(strerror(errno)));
	}

	pthread_attr_destroy(&attribs);

	NewThread = thread_to_init;
	NewThread->Creator = this;
	NewThread->Extend("pthread", MyPThread);

	Mutex(false);
}

PThreadEngine::~PThreadEngine()
{
}

void PThreadEngine::Run()
{
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

void PThreadEngine::FreeThread(Thread* thread)
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

