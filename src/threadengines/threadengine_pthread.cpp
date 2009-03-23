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

PThreadEngine::PThreadEngine(InspIRCd* Instance) : ThreadEngine(Instance)
{
}

static void* entry_point(void* parameter)
{
	/* Recommended by nenolod, signal safety on a per-thread basis */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	Thread* pt = reinterpret_cast<Thread*>(parameter);
	pt->Run();
	return parameter;
}


void PThreadEngine::Start(Thread* thread)
{
	PThreadData* data = new PThreadData;
	thread->state = data;

	if (pthread_create(&data->pthread_id, NULL, entry_point, thread) != 0)
	{
		thread->state = NULL;
		delete data;
		throw CoreException("Unable to create new PThreadEngine: " + std::string(strerror(errno)));
	}
}

PThreadEngine::~PThreadEngine()
{
}

void PThreadData::FreeThread(Thread* thread)
{
	thread->SetExitFlag(true);
	pthread_join(pthread_id, NULL);
}

MutexFactory::MutexFactory(InspIRCd* Instance) : ServerInstance(Instance)
{
}

Mutex* MutexFactory::CreateMutex()
{
	return new PosixMutex();
}

PosixMutex::PosixMutex() : Mutex()
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
