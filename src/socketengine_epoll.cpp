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
#include "globals.h"
#include "inspircd.h"
#include <sys/epoll.h>
#include <vector>
#include <string>
#include "socketengine_epoll.h"
#include "helperfuncs.h"
#include "inspircd.h"

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

bool EPollEngine::AddFd(int fd, bool readable, char type)
{
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

	ref[fd] = type;

	if (readable)
	{
		ServerInstance->Log(DEBUG,"Set readbit");
		ref[fd] |= X_READBIT;
	}
	ServerInstance->Log(DEBUG,"Add socket %d",fd);

	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	ServerInstance->Log(DEBUG,"epoll: Add socket to events, ep=%d socket=%d",EngineHandle,fd);
	readable ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
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

bool EPollEngine::DelFd(int fd)
{
	ServerInstance->Log(DEBUG,"EPollEngine::DelFd(%d)",fd);

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	ref[fd] && X_READBIT ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);
	if (i < 0)
	{
		ServerInstance->Log(DEBUG,"epoll: List deletion failure!");
		return false;
	}

	CurrentSetSize--;
	ref[fd] = 0;
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

int EPollEngine::Wait(int* fdlist)
{
	int result = 0;

	int i = epoll_wait(EngineHandle, events, MAX_DESCRIPTORS, 50);
	for (int j = 0; j < i; j++)
		fdlist[result++] = events[j].data.fd;

	return result;
}

std::string EPollEngine::GetName()
{
	return "epoll";
}
