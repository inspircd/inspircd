/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef THREADENGINE_PTHREAD_H
#define THREADENGINE_PTHREAD_H

#include <pthread.h>

class ThreadSignalSocket;

/** The Mutex class represents a mutex, which can be used to keep threads
 * properly synchronised. Use mutexes sparingly, as they are a good source
 * of thread deadlocks etc, and should be avoided except where absolutely
 * neccessary. Note that the internal behaviour of the mutex varies from OS
 * to OS depending on the thread engine, for example in windows a Mutex
 * in InspIRCd uses critical sections, as they are faster and simpler to
 * manage.
 */
class CoreExport Mutex
{
 private:
	pthread_mutex_t mutex;
	friend class pthread_cond_var;
 public:
	Mutex()
	{
		pthread_mutex_init(&mutex, NULL);
	}
	void lock()
	{
		pthread_mutex_lock(&mutex);
	}
	void unlock()
	{
		pthread_mutex_unlock(&mutex);
	}
	~Mutex()
	{
		pthread_mutex_destroy(&mutex);
	}

	/** RAII locking object for proper unlock on exceptions */
	class Lock
	{
	 public:
		Mutex& mutex;
		Lock(Mutex& m) : mutex(m)
		{
			mutex.lock();
		}
		~Lock()
		{
			mutex.unlock();
		}
	};
};

/** Only available in pthreads model */
class CoreExport pthread_cond_var
{
 public:
	pthread_cond_t pcond;
	pthread_cond_var()
	{
		pthread_cond_init(&pcond, NULL);
	}

	~pthread_cond_var()
	{
		pthread_cond_destroy(&pcond);
	}

	void wait(Mutex& mutex)
	{
		pthread_cond_wait(&pcond, &mutex.mutex);
	}
	
	void signal_one()
	{
		pthread_cond_signal(&pcond);
	}
};

class CoreExport ThreadEngine
{
 public:
	ThreadEngine();
	~ThreadEngine();
	void Submit(Job*);
	/** Wait for all jobs that rely on this module */
	void BlockForUnload(Module* going);

 private:
	class Runner : public classbase
	{
	 public:
		pthread_t id;
		ThreadEngine* const te;
		Job* current;
		static void* entry_point(void* parameter);
		void main_loop();
		Runner(ThreadEngine* t);
		~Runner();
	};

	void result_loop();

	Mutex job_lock;

	std::list<Job*> submit_q;
	pthread_cond_var submit_s;
	
	std::vector<Runner*> threads;
	
	std::list<Job*> result_q;
	pthread_cond_var result_sc;
	ThreadSignalSocket* result_ss;
	
	friend class ThreadSignalSocket;
};


#endif
