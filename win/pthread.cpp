/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * POSIX emulation layer for Windows.
 *
 *   Copyright (C) 2012 Anope Team <team@anope.org>
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

#include "pthread.h"

struct ThreadInfo
{
	void *(*entry)(void *);
	void *param;
};

static DWORD WINAPI entry_point(void *parameter)
{
	ThreadInfo *ti = static_cast<ThreadInfo *>(parameter);
	ti->entry(ti->param);
	delete ti;
	return 0;
}

int pthread_attr_init(pthread_attr_t *)
{
	/* No need for this */
	return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *, int)
{
	/* No need for this */
	return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *, void *(*entry)(void *), void *param)
{
	ThreadInfo *ti = new ThreadInfo;
	ti->entry = entry;
	ti->param = param;

	*thread = CreateThread(NULL, 0, entry_point, ti, 0, NULL);
	if (!*thread)
	{
		delete ti;
		return -1;
	}

	return 0;
}

int pthread_join(pthread_t thread, void **)
{
	if (WaitForSingleObject(thread, INFINITE) == WAIT_FAILED)
		return -1;
	return 0;
}

void pthread_exit(int i)
{
	ExitThread(i);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *)
{
	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	return !TryEnterCriticalSection(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *)
{
	*cond = CreateEvent(NULL, false, false, NULL);
	if (*cond == NULL)
		return -1;
	return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	return !CloseHandle(*cond);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	return !PulseEvent(*cond);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	LeaveCriticalSection(mutex);
	WaitForSingleObject(*cond, INFINITE);
	EnterCriticalSection(mutex);
	return 0;
}
