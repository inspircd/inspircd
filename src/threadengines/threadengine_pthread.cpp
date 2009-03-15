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

#include "inspircd.h"
#include "threadengines/threadengine_pthread.h"
#include <pthread.h>
#include <signal.h>

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

	/* Create a thread in a mutex. This prevents whacking the member value NewThread,
	 * and also prevents recursive creation of threads by mistake (instead, the thread
	 * will just deadlock itself)
	 */
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

	/* Always unset a mutex if you set it */
	Mutex(false);

	/* Wait for the PThreadEngine::Run method to take a copy of the
	 * pointer and clear this member value
	 */
	while (NewThread)
		usleep(1000);
}

PThreadEngine::~PThreadEngine()
{
}

void PThreadEngine::Run()
{
	/* Take a copy of the member value, then clear it. Do this
	 * in a mutex so that we can be sure nothing else is looking
	 * at it.
	 */
	Mutex(true);
	Thread* nt = NewThread;
	NewThread = NULL;
	Mutex(false);
	/* Now we have our own safe copy, call the object on it */
	nt->Run();
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
	/* Recommended by nenolod, signal safety on a per-thread basis */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL))
		signal(SIGPIPE, SIG_IGN);

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

MutexFactory::MutexFactory(InspIRCd* Instance) : ServerInstance(Instance)
{
}

Mutex* MutexFactory::CreateMutex()
{
	return new PosixMutex(this->ServerInstance);
}

PosixMutex::PosixMutex(InspIRCd* Instance) : Mutex(Instance)
{
	pthread_mutex_init(&putex, NULL);
}

PosixMutex::~PosixMutex()
{
	pthread_mutex_destroy(&putex);
}

void PosixMutex::Enable(bool enable)
{
	if (enable)
		pthread_mutex_lock(&putex);
	else
		pthread_mutex_unlock(&putex);
}
