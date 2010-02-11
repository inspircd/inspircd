/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include "socketengines/socketengine_epoll.h"
#include <ulimit.h>

EPollEngine::EPollEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	MAX_DESCRIPTORS = 0;
	// This is not a maximum, just a hint at the eventual number of sockets that may be polled.
	EngineHandle = epoll_create(GetMaxFds() / 4);

	if (EngineHandle == -1)
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT, "ERROR: Could not initialize socket engine: %s", strerror(errno));
		ServerInstance->Logs->Log("SOCKET",DEFAULT, "ERROR: Your kernel probably does not have the proper features. This is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine: %s\n", strerror(errno));
		printf("ERROR: Your kernel probably does not have the proper features. This is a fatal error, exiting now.\n");
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}
	CurrentSetSize = 0;

	ref = new EventHandler* [GetMaxFds()];
	events = new struct epoll_event[GetMaxFds()];

	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
}

EPollEngine::~EPollEngine()
{
	this->Close(EngineHandle);
	delete[] ref;
	delete[] events;
}

bool EPollEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"AddFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	if (GetRemainingFds() <= 1)
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"No remaining FDs cannot add fd: %d", fd);
		return false;
	}

	if (ref[fd])
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Attempt to add duplicate fd: %d", fd);
		return false;
	}

	struct epoll_event ev;
	memset(&ev,0,sizeof(ev));
	eh->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_ADD, fd, &ev);
	if (i < 0)
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Error adding fd: %d to socketengine: %s", fd, strerror(errno));
		return false;
	}

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);

	ref[fd] = eh;
	CurrentSetSize++;
	return true;
}

void EPollEngine::WantWrite(EventHandler* eh)
{
	/** Use oneshot so that the system removes the writeable
	 * status for us and saves us a call.
	 */
	struct epoll_event ev;
	memset(&ev,0,sizeof(ev));
	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.fd = eh->GetFd();
	epoll_ctl(EngineHandle, EPOLL_CTL_MOD, eh->GetFd(), &ev);
}

bool EPollEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"DelFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	struct epoll_event ev;
	memset(&ev,0,sizeof(ev));
	eh->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);

	if (i < 0 && !force)
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Cant remove socket: %s", strerror(errno));
		return false;
	}

	ref[fd] = NULL;
	CurrentSetSize--;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

int EPollEngine::GetMaxFds()
{
	if (MAX_DESCRIPTORS)
		return MAX_DESCRIPTORS;

	int max = ulimit(4, 0);
	if (max > 0)
	{
		MAX_DESCRIPTORS = max;
		return max;
	}
	else
	{
		ServerInstance->Logs->Log("SOCKET", DEFAULT, "ERROR: Can't determine maximum number of open sockets!");
		printf("ERROR: Can't determine maximum number of open sockets!\n");
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}
	return 0;
}

int EPollEngine::GetRemainingFds()
{
	return GetMaxFds() - CurrentSetSize;
}

int EPollEngine::DispatchEvents()
{
	socklen_t codesize = sizeof(int);
	int errcode;
	int i = epoll_wait(EngineHandle, events, GetMaxFds() - 1, 1000);

	TotalEvents += i;

	for (int j = 0; j < i; j++)
	{
		if (!ref[events[j].data.fd])
		{
			ServerInstance->Logs->Log("SOCKET",DEBUG,"Got event on unknown fd: %d", events[j].data.fd);
			epoll_ctl(EngineHandle, EPOLL_CTL_DEL, events[j].data.fd, &events[j]);
			continue;
		}
		if (events[j].events & EPOLLHUP)
		{
			ErrorEvents++;
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_ERROR, 0);
			continue;
		}
		if (events[j].events & EPOLLERR)
		{
			ErrorEvents++;
			/* Get error number */
			if (getsockopt(events[j].data.fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
				errcode = errno;
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_ERROR, errcode);
			continue;
		}
		if (events[j].events & EPOLLOUT)
		{
			WriteEvents++;
			struct epoll_event ev;
			memset(&ev,0,sizeof(ev));
			ev.events = EPOLLIN;
			ev.data.fd = events[j].data.fd;
			epoll_ctl(EngineHandle, EPOLL_CTL_MOD, events[j].data.fd, &ev);
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_WRITE);
		}
		else
		{
			ReadEvents++;
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_READ);
		}
	}

	return i;
}

std::string EPollEngine::GetName()
{
	return "epoll";
}

