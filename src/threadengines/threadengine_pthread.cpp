/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
	int fd = eventfd(0, EFD_NONBLOCK);
	if (fd < 0)
		throw new CoreException("Could not create pipe " + std::string(strerror(errno)));
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
	int fds[2];
	if (pipe(fds))
		throw new CoreException("Could not create pipe " + std::string(strerror(errno)));
	signal.sock = new ThreadSignalSocket(this, fds[0], fds[1]);
}
#endif

void SocketThread::NotifyParent()
{
	signal.sock->Notify();
}

SocketThread::~SocketThread()
{
}
