/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengines/socketengine_kqueue.h"
#include <ulimit.h>

KQueueEngine::KQueueEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	this->RecoverFromFork();
	ref = new EventHandler* [GetMaxFds()];
	ke_list = new struct kevent[GetMaxFds()];
}

void KQueueEngine::RecoverFromFork()
{
	/*
	 * The only bad thing about kqueue is that its fd cant survive a fork and is not inherited.
	 * BUM HATS.
	 * 
	 */
	EngineHandle = kqueue();
	if (EngineHandle == -1)
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT, "ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		ServerInstance->Logs->Log("SOCKET",DEFAULT, "ERROR: this is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.\n");
		printf("ERROR: this is a fatal error, exiting now.\n");
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}
	CurrentSetSize = 0;
	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
}

KQueueEngine::~KQueueEngine()
{
	this->Close(EngineHandle);
	delete[] ref;
	delete[] ke_list;
}

bool KQueueEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	struct kevent ke;
	EV_SET(&ke, fd, eh->Readable() ? EVFILT_READ : EVFILT_WRITE, EV_ADD, 0, 0, NULL);

	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		return false;
	}

	ref[fd] = eh;
	CurrentSetSize++;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);
	return true;
}

bool KQueueEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	struct kevent ke;
	EV_SET(&ke, eh->GetFd(), EVFILT_READ, EV_DELETE, 0, 0, NULL);

	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

	int j = kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	if ((j < 0) && (i < 0) && !force)
		return false;

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
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
	kevent(EngineHandle, &ke, 1, 0, 0, NULL);
}

int KQueueEngine::GetMaxFds()
{
	if (!MAX_DESCRIPTORS)
	{
		int mib[2], maxfiles;
	   	size_t len;

		mib[0] = CTL_KERN;
		mib[1] = KERN_MAXFILES;
		len = sizeof(maxfiles);
		sysctl(mib, 2, &maxfiles, &len, NULL, 0);

		MAX_DESCRIPTORS = maxfiles;
		return max;
	}
	return MAX_DESCRIPTORS;
}

int KQueueEngine::GetRemainingFds()
{
	return GetMaxFds() - CurrentSetSize;
}

int KQueueEngine::DispatchEvents()
{
	ts.tv_nsec = 0;
	ts.tv_sec = 1;

	int i = kevent(EngineHandle, NULL, 0, &ke_list[0], GetMaxFds(), &ts);

	TotalEvents += i;

	for (int j = 0; j < i; j++)
	{
		if (ke_list[j].flags & EV_EOF)
		{
			/* We love you kqueue, oh yes we do *sings*!
			 * kqueue gives us the error number directly in the EOF state!
			 * Unlike smelly epoll and select, where we have to getsockopt
			 * to get the error, this saves us time and cpu cycles. Go BSD!
			 */
			ErrorEvents++;
			if (ref[ke_list[j].ident])
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
			kevent(EngineHandle, &ke, 1, 0, 0, NULL);
			WriteEvents++;
			if (ref[ke_list[j].ident])
				ref[ke_list[j].ident]->HandleEvent(EVENT_WRITE);
		}
		else
		{
			ReadEvents++;
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
