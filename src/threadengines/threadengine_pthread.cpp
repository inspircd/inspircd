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
#include "threadengines/threadengine_pthread.h"
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

ThreadEngine::ThreadEngine()
{
}

static void* entry_point(void* parameter)
{
	/* Recommended by nenolod, signal safety on a per-thread basis */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	Thread* pt = static_cast<Thread*>(parameter);
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
	thread->SetExitFlag();
	pthread_join(pthread_id, NULL);
}

#ifdef HAS_EVENTFD
#include <sys/eventfd.h>

class ThreadSignalSocket : public EventHandler
{
	SocketThread* parent;
 public:
	ThreadSignalSocket(SocketThread* p, int newfd) : parent(p)
	{
		SetFd(newfd);
		ServerInstance->SE->AddFd(this, FD_WANT_FAST_READ | FD_WANT_NO_WRITE);
	}

	~ThreadSignalSocket()
	{
		ServerInstance->SE->DelFd(this);
		ServerInstance->SE->Close(GetFd());
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
			parent->OnNotify();
		}
		else
		{
			ServerInstance->GlobalCulls.AddItem(this);
		}
	}
};

SocketThread::SocketThread()
{
	signal.sock = NULL;
	int fd = eventfd(0, EFD_NONBLOCK);
	if (fd < 0)
		throw CoreException("Could not create pipe " + std::string(strerror(errno)));
	signal.sock = new ThreadSignalSocket(this, fd);
}
#else

class ThreadSignalSocket : public EventHandler
{
	SocketThread* parent;
	int send_fd;
 public:
	ThreadSignalSocket(SocketThread* p, int recvfd, int sendfd) :
		parent(p), send_fd(sendfd)
	{
		SetFd(recvfd);
		ServerInstance->SE->NonBlocking(fd);
		ServerInstance->SE->AddFd(this, FD_WANT_FAST_READ | FD_WANT_NO_WRITE);
	}

	~ThreadSignalSocket()
	{
		close(send_fd);
		ServerInstance->SE->DelFd(this);
		ServerInstance->SE->Close(GetFd());
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
			parent->OnNotify();
		}
		else
		{
			ServerInstance->GlobalCulls.AddItem(this);
		}
	}
};

SocketThread::SocketThread()
{
	signal.sock = NULL;
	int fds[2];
	if (pipe(fds))
		throw CoreException("Could not create pipe " + std::string(strerror(errno)));
	signal.sock = new ThreadSignalSocket(this, fds[0], fds[1]);
}
#endif

void SocketThread::NotifyParent()
{
	signal.sock->Notify();
}

SocketThread::~SocketThread()
{
	if (signal.sock)
	{
		signal.sock->cull();
		delete signal.sock;
	}
}
