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


#include "inspircd.h"
#include "cull_list.h"
#include "threadengine.h"
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#ifdef HAS_EVENTFD
#include <sys/eventfd.h>

class ThreadSignalSocket : public EventHandler
{
 public:
	ThreadSignalSocket()
	{
		SetFd(eventfd(0, EFD_NONBLOCK));
		if (fd < 0)
			throw CoreException("Could not create eventfd " + std::string(strerror(errno)));
		ServerInstance->SE->AddFd(this, FD_WANT_FAST_READ | FD_WANT_NO_WRITE);
	}

	~ThreadSignalSocket()
	{
		close(fd);
	}

	void Notify()
	{
		eventfd_write(fd, 1);
	}

	void HandleEvent(EventType et, int errornum)
	{
		if (et == EVENT_READ)
		{
			eventfd_t dummy;
			eventfd_read(fd, &dummy);
			ServerInstance->Threads->result_loop();
		}
		else
		{
			ServerInstance->Threads->job_lock.lock();
			ServerInstance->Threads->result_ss = NULL;
			ServerInstance->Threads->job_lock.unlock();
			ServerInstance->GlobalCulls->AddItem(this);
		}
	}
};

#else

class ThreadSignalSocket : public EventHandler
{
	int send_fd;
 public:
	ThreadSignalSocket()
	{
		int fds[2];
		if (pipe(fds))
			throw CoreException("Could not create pipe " + std::string(strerror(errno)));
		SetFd(fds[0]);
		send_fd = fds[1];

		ServerInstance->SE->NonBlocking(fd);
		ServerInstance->SE->AddFd(this, FD_WANT_FAST_READ | FD_WANT_NO_WRITE);
	}

	~ThreadSignalSocket()
	{
		close(send_fd);
		close(fd);
	}

	void Notify()
	{
		static const char dummy = '*';
		write(send_fd, &dummy, 1);
	}

	void HandleEvent(EventType et, int errornum)
	{
		if (et == EVENT_READ)
		{
			char dummy[128];
			read(fd, dummy, 128);
			ServerInstance->Threads->result_loop();
		}
		else
		{
			ServerInstance->Threads->job_lock.lock();
			ServerInstance->Threads->result_ss = NULL;
			ServerInstance->Threads->job_lock.unlock();
			ServerInstance->GlobalCulls->AddItem(this);
		}
	}
};
#endif

ThreadEngine::ThreadEngine() : result_ss(NULL)
{
}

ThreadEngine::~ThreadEngine()
{
	delete result_ss;
}

void* ThreadEngine::Runner::entry_point(void* parameter)
{
#ifndef WINDOWS
	/* Recommended by nenolod, signal safety on a per-thread basis */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

	Runner* te = static_cast<Runner*>(parameter);
	te->main_loop();
	return parameter;
}

ThreadEngine::Runner::Runner(ThreadEngine* t)
	: te(t), current(NULL)
{
	if (pthread_create(&id, NULL, entry_point, this) != 0)
		throw CoreException("Unable to create new thread: " + std::string(strerror(errno)));
}

ThreadEngine::Runner::~Runner()
{
	pthread_join(id, NULL);
}

void ThreadEngine::Submit(Job* job)
{
	Mutex::Lock lock(job_lock);
	// TODO launch more threads if we have a queue and more are allowed
	// TODO clean up threads at garbage collection time (all but one)
	if (threads.empty())
		threads.push_back(new Runner(this));
	if (result_ss == NULL)
		result_ss = new ThreadSignalSocket();
	submit_q.push_back(job);
	submit_s.signal_one();
}

static class TerminateJob : public Job
{
 public:
	TerminateJob() : Job(NULL) { cancel(); }
	void run() {}
	void finish() {}
} terminator;

void ThreadEngine::Runner::main_loop()
{
	te->job_lock.lock();
	while (1)
	{
		while (te->submit_q.empty())
			te->submit_s.wait(te->job_lock);

		current = te->submit_q.front();
		te->submit_q.pop_front();

		if (!current->IsCancelled())
		{
			te->job_lock.unlock();
			try
			{
				current->run();
			}
			catch (...)
			{
			}
			te->job_lock.lock();
		}
		te->result_q.push_back(current);
		if (current == &terminator)
			break;
		current = NULL;

		te->result_sc.signal_one();
		if (te->result_ss)
			te->result_ss->Notify();
	}
	te->job_lock.unlock();
	// current == terminator, we die
}

void ThreadEngine::result_loop()
{
	job_lock.lock();
	while (!result_q.empty())
	{
		Job* job = result_q.front();
		result_q.pop_front();
		job_lock.unlock();
		job->finish();
		job_lock.lock();
	}
	job_lock.unlock();
}

void ThreadEngine::BlockForUnload(Module* mod)
{
	job_lock.lock();
	while (1)
	{
		bool found = false;
		for(std::list<Job*>::iterator i = submit_q.begin(); i != submit_q.end(); i++)
		{
			Job* j = *i;
			if (j->BlocksUnload(mod))
			{
				found = true;
				j->cancel();
			}
		}
		for(std::vector<Runner*>::iterator i = threads.begin(); i != threads.end(); i++)
		{
			Runner* r = *i;
			if (r->current && r->current->BlocksUnload(mod))
			{
				found = true;
				r->current->cancel();
			}
		}
		if (!found)
			break;
		// wait for an item to finish so we can check again
		result_sc.wait(job_lock);
	}
	job_lock.unlock();
	// clean up any references remaining in the result loop
	// (that's where any we were waiting for are sitting)
	result_loop();
}
