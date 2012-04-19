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
#include "threadengines/threadengine_win32.h"

ThreadEngine::ThreadEngine()
{
}

void ThreadEngine::Start(Thread* thread)
{
	ThreadData* data = new ThreadData;
	thread->state = data;

	DWORD ThreadId = 0;
	data->handle = CreateThread(NULL,0,ThreadEngine::Entry,thread,0,&ThreadId);

	if (data->handle == NULL)
	{
		thread->state = NULL;
		delete data;
		throw CoreException(std::string("Unable to create new thread: ") + dlerror());
	}
}

ThreadEngine::~ThreadEngine()
{
}

DWORD WINAPI ThreadEngine::Entry(void* parameter)
{
	Thread* pt = static_cast<Thread*>(parameter);
	pt->Run();
	return 0;
}

void ThreadData::FreeThread(Thread* thread)
{
	thread->SetExitFlag();
	WaitForSingleObject(handle,INFINITE);
}

class ThreadSignalSocket : public BufferedSocket
{
	SocketThread* parent;
 public:
	ThreadSignalSocket(SocketThread* t, int newfd)
		: BufferedSocket(newfd), parent(t)
	{
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
	int listenFD = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFD == -1)
		throw CoreException("Could not create ITC pipe");
	int connFD = socket(AF_INET, SOCK_STREAM, 0);
	if (connFD == -1)
		throw CoreException("Could not create ITC pipe");

	if (!ServerInstance->BindSocket(listenFD, 0, "127.0.0.1", true))
		throw CoreException("Could not create ITC pipe");
	ServerInstance->SE->NonBlocking(connFD);

	struct sockaddr_in addr;
	socklen_t sz = sizeof(addr);
	getsockname(listenFD, reinterpret_cast<struct sockaddr*>(&addr), &sz);
	connect(connFD, reinterpret_cast<struct sockaddr*>(&addr), sz);
	ServerInstance->SE->Blocking(listenFD);
	int nfd = accept(listenFD, reinterpret_cast<struct sockaddr*>(&addr), &sz);
	if (nfd < 0)
		throw CoreException("Could not create ITC pipe");
	new ThreadSignalSocket(this, nfd);
	closesocket(listenFD);

	ServerInstance->SE->Blocking(connFD);
	this->signal.connFD = connFD;
}

void SocketThread::NotifyParent()
{
	char dummy = '*';
	send(signal.connFD, &dummy, 1, 0);
}

SocketThread::~SocketThread()
{
	if (signal.connFD >= 0)
	{
		shutdown(signal.connFD, 2);
		closesocket(signal.connFD);
	}
}
