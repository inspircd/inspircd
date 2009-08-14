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
#include "threadengines/threadengine_win32.h"

ThreadEngine::ThreadEngine(InspIRCd* Instance)
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
	Thread* pt = reinterpret_cast<Thread*>(parameter);
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
	ThreadSignalSocket(SocketThread* t, InspIRCd* SI, int newfd, char* ip)
		: BufferedSocket(SI, newfd, ip), parent(t)
	{
	}
	
	virtual bool OnDataReady()
	{
		char data = 0;
		if (ServerInstance->SE->Recv(this, &data, 1, 0) > 0)
		{
			parent->OnNotify();
			return true;
		}
		return false;
	}
};

class ThreadSignalListener : public ListenSocketBase
{
	SocketThread* parent;
	irc::sockets::insp_sockaddr sock_us;
 public:
	ThreadSignalListener(SocketThread* t, InspIRCd* Instance, int port, const std::string &addr) : ListenSocketBase(Instance, port, addr), parent(t)
	{
		socklen_t uslen = sizeof(sock_us);
		if (getsockname(this->fd,(sockaddr*)&sock_us,&uslen))
		{
			throw ModuleException("Could not getsockname() to find out port number for ITC port");
		}
	}

	virtual void OnAcceptReady(const std::string &ipconnectedto, int nfd, const std::string &incomingip)
	{
		new ThreadSignalSocket(parent, ServerInstance, nfd, const_cast<char*>(ipconnectedto.c_str()));
		ServerInstance->SE->DelFd(this);
		// XXX unsafe casts suck
	}
/* Using getsockname and ntohs, we can determine which port number we were allocated */
	int GetPort()
	{
#ifdef IPV6
		return ntohs(sock_us.sin6_port);
#else
		return ntohs(sock_us.sin_port);
#endif
	}
};

SocketThread::SocketThread(InspIRCd* SI)
{
	ThreadSignalListener* listener = new ThreadSignalListener(this, SI, 0, "127.0.0.1");
	if (listener->GetFd() == -1)
		throw CoreException("Could not create ITC pipe");
	int connFD = socket(AF_INET, SOCK_STREAM, 0);
	if (connFD == -1)
		throw CoreException("Could not create ITC pipe");
	
	irc::sockets::insp_sockaddr addr;

#ifdef IPV6
	irc::sockets::insp_aton("::1", &addr.sin6_addr);
	addr.sin6_family = AF_INET6;
	addr.sin6_port = htons(listener->GetPort());
#else
	irc::sockets::insp_aton("127.0.0.1", &addr.sin_addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(listener->GetPort());
#endif

	if (connect(connFD, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		SI->SE->DelFd(listener);
		closesocket(connFD);
		throw CoreException("Could not connet to ITC pipe");
	}
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
