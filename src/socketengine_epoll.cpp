/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <sys/epoll.h>
#include "socketengine_epoll.h"

EPollEngine::EPollEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	EngineHandle = epoll_create(MAX_DESCRIPTORS);

	if (EngineHandle == -1)
	{
		ServerInstance->Log(SPARSE,"ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		ServerInstance->Log(SPARSE,"ERROR: this is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		printf("ERROR: this is a fatal error, exiting now.");
		InspIRCd::Exit(ERROR);
	}
	CurrentSetSize = 0;
}

EPollEngine::~EPollEngine()
{
	close(EngineHandle);
}

bool EPollEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Log(DEBUG,"ERROR: FD of %d added above max of %d",fd,MAX_DESCRIPTORS);
		return false;
	}
	if (GetRemainingFds() <= 1)
	{
		ServerInstance->Log(DEBUG,"ERROR: System out of file descriptors!");
		return false;
	}
	if (ref[fd])
	{
		ServerInstance->Log(DEBUG,"Slot %d already occupied",fd);
		return false;
	}

	ref[fd] = eh;

	ServerInstance->Log(DEBUG,"***** Add socket %d",fd);

	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	ServerInstance->Log(DEBUG,"epoll: Add socket to events, ep=%d socket=%d",EngineHandle,fd);
	eh->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_ADD, fd, &ev);
	if (i < 0)
	{
		ServerInstance->Log(DEBUG,"epoll: List insertion failure!");
		return false;
	}

	CurrentSetSize++;
	return true;
}

void EPollEngine::WantWrite(EventHandler* eh)
{
	/** Use oneshot so that the system removes the writeable
	 * status for us and saves us a call.
	 */
	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	ev.events = EPOLLOUT;
	ev.data.fd = eh->GetFd();
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_MOD, eh->GetFd(), &ev);
	if (i < 0)
	{
		ServerInstance->Log(DEBUG,"epoll: Could not set want write on fd %d!",eh->GetFd());
	}
}

bool EPollEngine::DelFd(EventHandler* eh)
{
	ServerInstance->Log(DEBUG,"EPollEngine::DelFd(%d)",eh->GetFd());

	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	eh->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);

	if (i < 0)
	{
		ServerInstance->Log(DEBUG,"epoll: List deletion failure: %s",strerror(errno));
		return false;
	}

	CurrentSetSize--;
	ref[fd] = NULL;

	return true;
}

int EPollEngine::GetMaxFds()
{
	return MAX_DESCRIPTORS;
}

int EPollEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int EPollEngine::DispatchEvents()
{
	socklen_t codesize;
	int errcode;
	int i = epoll_wait(EngineHandle, events, MAX_DESCRIPTORS, 150);
	for (int j = 0; j < i; j++)
	{
		ServerInstance->Log(DEBUG,"Handle %s event on fd %d",events[j].events & EPOLLOUT ? "write" : "read", events[j].data.fd);
		if (events[j].events & EPOLLHUP)
		{
			ServerInstance->Log(DEBUG,"Handle error event on fd %d", events[j].data.fd);
			ref[events[j].data.fd]->HandleEvent(EVENT_ERROR, 0);
			continue;
		}
		if (events[j].events & EPOLLERR)
		{
			/* Get error number */
			if (getsockopt(events[j].data.fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
				errcode = errno;
			ServerInstance->Log(DEBUG,"Handle error event on fd %d: %s", events[j].data.fd, strerror(errcode));
			ref[events[j].data.fd]->HandleEvent(EVENT_ERROR, errcode);
			continue;
		}
		if (events[j].events & EPOLLOUT)
		{
			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.fd = events[j].data.fd;
			int i = epoll_ctl(EngineHandle, EPOLL_CTL_MOD, events[j].data.fd, &ev);
			if (i < 0)
			{
				ServerInstance->Log(DEBUG,"epoll: Could not reset fd %d!", events[j].data.fd);
			}
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_WRITE);
		}
		else
		{
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

