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

#include "inspircd_config.h"
#include "inspircd.h"
#include <sys/epoll.h>
#include <vector>
#include <string>
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
		ServerInstance->Log(DEFAULT,"ERROR: FD of %d added above max of %d",fd,MAX_DESCRIPTORS);
		return false;
	}
	if (GetRemainingFds() <= 1)
	{
		ServerInstance->Log(DEFAULT,"ERROR: System out of file descriptors!");
		return false;
	}
	if (ref[fd])
		return false;

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

bool EPollEngine::DelFd(EventHandler* eh)
{
	ServerInstance->Log(DEBUG,"EPollEngine::DelFd(%d)",eh->GetFd());

	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	ref[fd]->Readable() ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);
	if (i < 0)
	{
		ServerInstance->Log(DEBUG,"epoll: List deletion failure!");
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
	int i = epoll_wait(EngineHandle, events, MAX_DESCRIPTORS, 50);
	for (int j = 0; j < i; j++)
	{
		ServerInstance->Log(DEBUG,"Handle %s event on fd %d",ref[events[j].data.fd]->Readable() ? "read" : "write", ref[events[j].data.fd]->GetFd());
		ref[events[j].data.fd]->HandleEvent(ref[events[j].data.fd]->Readable() ? EVENT_READ : EVENT_WRITE);
	}

	return i;
}

std::string EPollEngine::GetName()
{
	return "epoll";
}

