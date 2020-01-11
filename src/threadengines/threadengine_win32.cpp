/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009, 2011 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2008-2009 Craig Edwards <brain@inspircd.org>
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
#include "threadengines/threadengine_win32.h"

void ThreadEngine::Start(Thread* thread)
{
	thread->state.handle = CreateThread(NULL, 0, ThreadEngine::Entry, thread, 0, NULL);

	if (thread->state.handle == NULL)
	{
		DWORD lasterr = GetLastError();
		std::string err = "Unable to create new thread: " + ConvToStr(lasterr);
		SetLastError(ERROR_SUCCESS);
		throw CoreException(err);
	}
}

DWORD WINAPI ThreadEngine::Entry(void* parameter)
{
	Thread* pt = static_cast<Thread*>(parameter);
	pt->Run();
	return 0;
}

void ThreadEngine::Stop(Thread* thread)
{
	thread->SetExitFlag();
	HANDLE handle = thread->state.handle;
	WaitForSingleObject(handle,INFINITE);
	CloseHandle(handle);
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

static bool BindAndListen(int sockfd, int port, const char* addr)
{
	irc::sockets::sockaddrs servaddr;
	if (!irc::sockets::aptosa(addr, port, servaddr))
		return false;

	if (SocketEngine::Bind(sockfd, servaddr) != 0)
		return false;

	if (SocketEngine::Listen(sockfd, ServerInstance->Config->MaxConn) != 0)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "ERROR in listen(): %s", strerror(errno));
		return false;
	}

	return true;
}

SocketThread::SocketThread()
{
	int listenFD = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFD == -1)
		throw CoreException("Could not create ITC pipe");
	int connFD = socket(AF_INET, SOCK_STREAM, 0);
	if (connFD == -1)
		throw CoreException("Could not create ITC pipe");

	if (!BindAndListen(listenFD, 0, "127.0.0.1"))
		throw CoreException("Could not create ITC pipe");
	SocketEngine::NonBlocking(connFD);

	struct sockaddr_in addr;
	socklen_t sz = sizeof(addr);
	getsockname(listenFD, reinterpret_cast<struct sockaddr*>(&addr), &sz);
	connect(connFD, reinterpret_cast<struct sockaddr*>(&addr), sz);
	SocketEngine::Blocking(listenFD);
	int nfd = accept(listenFD, reinterpret_cast<struct sockaddr*>(&addr), &sz);
	if (nfd < 0)
		throw CoreException("Could not create ITC pipe");
	new ThreadSignalSocket(this, nfd);
	closesocket(listenFD);

	SocketEngine::Blocking(connFD);
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
