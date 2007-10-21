/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include <sys/epoll.h>
#include "socketengine_epoll.h"

EPollEngine::EPollEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	EngineHandle = epoll_create(MAX_DESCRIPTORS);

	if (EngineHandle == -1)
	{
		ServerInstance->Log(SPARSE,"ERROR: Could not initialize socket engine: %s", strerror(errno));
		ServerInstance->Log(SPARSE,"ERROR: Your kernel probably does not have the proper features. This is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine: %s\n", strerror(errno));
		printf("ERROR: Your kernel probably does not have the proper features. This is a fatal error, exiting now.\n");
		InspIRCd::Exit(EXIT_STATUS_SOCKETENGINE);
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
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	eh->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_ADD, fd, &ev);
	if (i < 0)
	{
		return false;
	}

	ServerInstance->Log(DEBUG,"New file descriptor: %d", fd);
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
	epoll_ctl(EngineHandle, EPOLL_CTL_MOD, eh->GetFd(), &ev);
}

bool EPollEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	eh->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);

	if (i < 0 && !force)
		return false;

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Log(DEBUG,"Remove file descriptor: %d", fd);
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
	int i = epoll_wait(EngineHandle, events, MAX_DESCRIPTORS, 1000);
	for (int j = 0; j < i; j++)
	{
		if (events[j].events & EPOLLHUP)
		{
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_ERROR, 0);
			continue;
		}
		if (events[j].events & EPOLLERR)
		{
			/* Get error number */
			if (getsockopt(events[j].data.fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
				errcode = errno;
			if (ref[events[j].data.fd])
				ref[events[j].data.fd]->HandleEvent(EVENT_ERROR, errcode);
			continue;
		}
		if (events[j].events & EPOLLOUT)
		{
			struct epoll_event ev;
			memset(&ev,0,sizeof(struct epoll_event));
			ev.events = EPOLLIN;
			ev.data.fd = events[j].data.fd;
			epoll_ctl(EngineHandle, EPOLL_CTL_MOD, events[j].data.fd, &ev);
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

