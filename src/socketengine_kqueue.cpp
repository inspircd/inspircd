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
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine_kqueue.h"


KQueueEngine::KQueueEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	EngineHandle = kqueue();
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

KQueueEngine::~KQueueEngine()
{
	ServerInstance->Log(DEBUG,"KQueueEngine::~KQueueEngine()");
	close(EngineHandle);
}

bool KQueueEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	ServerInstance->Log(DEBUG,"KQueueEngine::AddFd(%d)",fd);

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
	{
		ServerInstance->Log(DEFAULT,"ERROR: Slot already occupied");
		return false;
	}

	ref[fd] = eh;
	ServerInstance->Log(DEBUG,"Add socket %d",fd);

	struct kevent ke;
	ServerInstance->Log(DEBUG,"kqueue: Add socket to events, kq=%d socket=%d",EngineHandle,fd);
	EV_SET(&ke, fd, eh->Readable() ? EVFILT_READ : EVFILT_WRITE, EV_ADD, 0, 0, NULL);

	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		ServerInstance->Log(DEBUG,"kqueue: List insertion failure!");
		return false;
	}

	CurrentSetSize++;
	return true;
}

bool KQueueEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	ServerInstance->Log(DEBUG,"KQueueEngine::DelFd(%d)",fd);

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	struct kevent ke;
	EV_SET(&ke, eh->GetFd(), EVFILT_READ, EV_DELETE, 0, 0, NULL);

	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	
	EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

	int j = kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	if ((j < 0) && (i < 0))
		return false;

	CurrentSetSize--;
	ref[fd] = NULL;

	return true;
}

void KQueueEngine::WantWrite(EventHandler* eh)
{
	/** When changing an item in a kqueue, there is no 'modify' call
	 * as in epoll. Instead, we add the item again, and this overwrites
	 * the original setting rather than adding it twice. See man kqueue.
	 */
	struct kevent ke;
	EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		ServerInstance->Log(DEBUG,"kqueue: Unable to set fd %d for wanting write", eh->GetFd());
	}
}

int KQueueEngine::GetMaxFds()
{
	return MAX_DESCRIPTORS;
}

int KQueueEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int KQueueEngine::DispatchEvents()
{
	ts.tv_nsec = 5000L;
	ts.tv_sec = 0;
	int i = kevent(EngineHandle, NULL, 0, &ke_list[0], MAX_DESCRIPTORS, &ts);
	for (int j = 0; j < i; j++)
	{
		ServerInstance->Log(DEBUG,"Handle %s event on fd %d",ke_list[j].flags & EVFILT_WRITE ? "write" : "read", ke_list[j].ident);
		if (ke_list[j].flags & EV_EOF)
		{
			ServerInstance->Log(DEBUG,"kqueue: Error on FD %d", ke_list[j].ident);
			/* We love you kqueue, oh yes we do *sings*!
			 * kqueue gives us the error number directly in the EOF state!
			 * Unlike smelly epoll and select, where we have to getsockopt
			 * to get the error, this saves us time and cpu cycles. Go BSD!
			 */
			ref[ke_list[j].ident]->HandleEvent(EVENT_ERROR, ke_list[j].fflags);
			continue;
		}
		if (ke_list[j].flags & EVFILT_WRITE)
		{
			/* This looks wrong but its right. As above, theres no modify 
			 * call in kqueue. See the manpage.
			 */
			struct kevent ke;
			EV_SET(&ke, ke_list[j].ident, EVFILT_READ, EV_ADD, 0, 0, NULL);
			int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
			if (i == -1)
			{
				ServerInstance->Log(DEBUG,"kqueue: Unable to set fd %d back to just wanting to read!", ke_list[j].ident);
			}
			if (ref[ke_list[j].ident])
				ref[ke_list[j].ident]->HandleEvent(EVENT_WRITE);
		}
		else
		{
			if (ref[ke_list[j].ident])
				ref[ke_list[j].ident]->HandleEvent(EVENT_READ);
		}
	}

	return i;
}

std::string KQueueEngine::GetName()
{
	return "kqueue";
}
