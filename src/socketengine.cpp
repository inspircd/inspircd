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
#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
#ifdef USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#include <vector>
#include <string>
#include "socketengine.h"

char ref[MAX_DESCRIPTORS];

SocketEngine::SocketEngine()
{
	log(DEBUG,"SocketEngine::SocketEngine()");
#ifdef USE_EPOLL
	EngineHandle = epoll_create(MAX_DESCRIPTORS);
#endif
#ifdef USE_KQUEUE
	EngineHandle = kqueue();
#endif
#ifdef USE_SELECT
	EngineHandle = 0;
#endif
	if (EngineHandle == -1)
	{
		log(SPARSE,"ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		log(SPARSE,"ERROR: this is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		printf("ERROR: this is a fatal error, exiting now.");
		Exit(0);
	}
	CurrentSetSize = 0;
}

SocketEngine::~SocketEngine()
{
	log(DEBUG,"SocketEngine::~SocketEngine()");
#ifdef USE_EPOLL
	close(EngineHandle);
#endif
#ifdef USE_KQUEUE
	close(EngineHandle);
#endif
}

char SocketEngine::GetType(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return X_EMPTY_SLOT;
	/* Mask off the top bit used for 'read/write' state */
	return (ref[fd] & ~0x80);
}

bool SocketEngine::AddFd(int fd, bool readable, char type)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		log(DEFAULT,"ERROR: FD of %d added above max of %d",fd,MAX_DESCRIPTORS);
		return false;
	}
	if (GetRemainingFds() <= 1)
	{
		log(DEFAULT,"ERROR: System out of file descriptors!");
		return false;
	}
#ifdef USE_SELECT
	fds[fd] = fd;
#endif
	if (ref[fd])
		return false;

	ref[fd] = type;
	if (readable)
	{
		log(DEBUG,"Set readbit");
		ref[fd] |= X_READBIT;
	}
	log(DEBUG,"Add socket %d",fd);
#ifdef USE_EPOLL
	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	log(DEBUG,"epoll: Add socket to events, ep=%d socket=%d",EngineHandle,fd);
	readable ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_ADD, fd, &ev);
	if (i < 0)
	{
		log(DEBUG,"epoll: List insertion failure!");
		return false;
	}
#endif
#ifdef USE_KQUEUE
	struct kevent ke;
	log(DEBUG,"kqueue: Add socket to events, kq=%d socket=%d",EngineHandle,fd);
	EV_SET(&ke, fd, readable ? EVFILT_READ : EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		log(DEBUG,"kqueue: List insertion failure!");
		return false;
	}
#endif
	CurrentSetSize++;
	return true;
}

bool SocketEngine::HasFd(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;
	return (ref[fd] != 0);
}

bool SocketEngine::DelFd(int fd)
{
	log(DEBUG,"SocketEngine::DelFd(%d)",fd);

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

#ifdef USE_SELECT
	std::map<int,int>::iterator t = fds.find(fd);
	if (t != fds.end())
	{
		fds.erase(t);
		log(DEBUG,"Deleted fd %d",fd);
	}
#endif
#ifdef USE_KQUEUE
	struct kevent ke;
	EV_SET(&ke, fd, ref[fd] & X_READBIT ? EVFILT_READ : EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		log(DEBUG,"kqueue: Failed to remove socket from queue!");
		return false;
	}
#endif
#ifdef USE_EPOLL
	struct epoll_event ev;
	memset(&ev,0,sizeof(struct epoll_event));
	ref[fd] && X_READBIT ? ev.events = EPOLLIN : ev.events = EPOLLOUT;
	ev.data.fd = fd;
	int i = epoll_ctl(EngineHandle, EPOLL_CTL_DEL, fd, &ev);
	if (i < 0)
	{
		log(DEBUG,"epoll: List deletion failure!");
		return false;
	}
#endif
	CurrentSetSize--;
	ref[fd] = 0;
	return true;
}

int SocketEngine::GetMaxFds()
{
#ifdef USE_SELECT
	return FD_SETSIZE;
#endif
#ifdef USE_KQUEUE
	return MAX_DESCRIPTORS;
#endif
#ifdef USE_EPOLL
	return MAX_DESCRIPTORS;
#endif
}

int SocketEngine::GetRemainingFds()
{
#ifdef USE_SELECT
	return FD_SETSIZE - CurrentSetSize;
#endif
#ifdef USE_KQUEUE
	return MAX_DESCRIPTORS - CurrentSetSize;
#endif
#ifdef USE_EPOLL
	return MAX_DESCRIPTORS - CurrentSetSize;
#endif
}

int SocketEngine::Wait(int* fdlist)
{
	int result = 0;
#ifdef USE_SELECT
	FD_ZERO(&wfdset);
	FD_ZERO(&rfdset);
	timeval tval;
	int sresult;
	for (std::map<int,int>::iterator a = fds.begin(); a != fds.end(); a++)
	{
		if (ref[a->second] & X_READBIT)
		{
			FD_SET (a->second, &rfdset);
		}
		else
		{
			FD_SET (a->second, &wfdset);
		}
		
	}
	tval.tv_sec = 0;
	tval.tv_usec = 50L;
	sresult = select(FD_SETSIZE, &rfdset, &wfdset, NULL, &tval);
	if (sresult > 0)
	{
		for (std::map<int,int>::iterator a = fds.begin(); a != fds.end(); a++)
		{
			if ((FD_ISSET (a->second, &rfdset)) || (FD_ISSET (a->second, &wfdset)))
				fdlist[result++] = a->second;
		}
	}
#endif
#ifdef USE_KQUEUE
	ts.tv_nsec = 5000L;
	ts.tv_sec = 0;
	int i = kevent(EngineHandle, NULL, 0, &ke_list[0], MAX_DESCRIPTORS, &ts);
	for (int j = 0; j < i; j++)
		fdlist[result++] = ke_list[j].ident;
#endif
#ifdef USE_EPOLL
	int i = epoll_wait(EngineHandle, events, MAX_DESCRIPTORS, 50);
	for (int j = 0; j < i; j++)
		fdlist[result++] = events[j].data.fd;
#endif
	return result;
}

std::string SocketEngine::GetName()
{
#ifdef USE_SELECT
	return "select";
#endif
#ifdef USE_KQUEUE
	return "kqueue";
#endif
#ifdef USE_EPOLL
	return "epoll";
#endif
	return "misconfigured";
}
