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

ThreadEngine::ThreadEngine(InspIRCd* Instance)
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


void ThreadEngine::Start(Thread* thread)
{
	ThreadData* data = new ThreadData;
	thread->state = data;

	if (pthread_create(&data->pthread_id, NULL, entry_point, thread) != 0)
	{
		thread->state = NULL;
		delete data;
		throw CoreException("Unable to create new thread: " + std::string(strerror(errno)));
	}
}

ThreadEngine::~ThreadEngine()
{
}

void ThreadData::FreeThread(Thread* thread)
{
	thread->SetExitFlag(true);
	pthread_join(pthread_id, NULL);
}
