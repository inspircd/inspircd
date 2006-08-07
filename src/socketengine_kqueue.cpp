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
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <vector>
#include <string>
#include "socketengine_kqueue.h"
#include "helperfuncs.h"

KQueueEngine::KQueueEngine()
{
	EngineHandle = kqueue();
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

KQueueEngine::~KQueueEngine()
{
	log(DEBUG,"KQueueEngine::~KQueueEngine()");
	close(EngineHandle);
}

bool KQueueEngine::AddFd(int fd, bool readable, char type)
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

	if (ref[fd])
		return false;

	ref[fd] = type;
	if (readable)
	{
		log(DEBUG,"Set readbit");
		ref[fd] |= X_READBIT;
	}
	log(DEBUG,"Add socket %d",fd);

	struct kevent ke;
	log(DEBUG,"kqueue: Add socket to events, kq=%d socket=%d",EngineHandle,fd);
	EV_SET(&ke, fd, readable ? EVFILT_READ : EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		log(DEBUG,"kqueue: List insertion failure!");
		return false;
	}

	CurrentSetSize++;
	return true;
}

bool KQueueEngine::DelFd(int fd)
{
	log(DEBUG,"KQueueEngine::DelFd(%d)",fd);

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	struct kevent ke;
	EV_SET(&ke, fd, ref[fd] & X_READBIT ? EVFILT_READ : EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		log(DEBUG,"kqueue: Failed to remove socket from queue!");
		return false;
	}

	CurrentSetSize--;
	ref[fd] = 0;
	return true;
}

int KQueueEngine::GetMaxFds()
{
	return MAX_DESCRIPTORS;
}

int KQueueEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int KQueueEngine::Wait(int* fdlist)
{
	int result = 0;

	ts.tv_nsec = 5000L;
	ts.tv_sec = 0;
	int i = kevent(EngineHandle, NULL, 0, &ke_list[0], MAX_DESCRIPTORS, &ts);
	for (int j = 0; j < i; j++)
		fdlist[result++] = ke_list[j].ident;

	return result;
}

std::string KQueueEngine::GetName()
{
	return "kqueue";
}
