/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

class ThreadSignalSocket : public BufferedSocket
{
	SocketThread* parent;
 public:
	ThreadSignalSocket(SocketThread* p, int newfd) : BufferedSocket(newfd), parent(p) {}

	~ThreadSignalSocket()
	{
	}

	void Notify()
	{
		eventfd_write(fd, 1);
	}

	void OnDataReady()
	{
		recvq.clear();
		parent->OnNotify();
	}

	void OnError(BufferedSocketError)
	{
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

SocketThread::SocketThread()
{
	int fd = eventfd(0, O_NONBLOCK);
	if (fd < 0)
		throw new CoreException("Could not create pipe " + std::string(strerror(errno)));
	signal.sock = new ThreadSignalSocket(this, fd);
}
#else

class ThreadSignalSocket : public BufferedSocket
{
	SocketThread* parent;
	int send_fd;
 public:
	ThreadSignalSocket(SocketThread* p, int recvfd, int sendfd) :
		BufferedSocket(recvfd), parent(p), send_fd(sendfd)  {}

	~ThreadSignalSocket()
	{
		close(send_fd);
	}

	void Notify()
	{
		char dummy = '*';
		write(send_fd, &dummy, 1);
	}

	void OnDataReady()
	{
		recvq.clear();
		parent->OnNotify();
	}

	void OnError(BufferedSocketError)
	{
		ServerInstance->GlobalCulls.AddItem(this);
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
