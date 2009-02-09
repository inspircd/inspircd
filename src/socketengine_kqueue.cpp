/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine_kqueue.h"


KQueueEngine::KQueueEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	EngineHandle = kqueue();
	if (EngineHandle == -1)
	{
		ServerInstance->Log(DEFAULT, "ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		ServerInstance->Log(DEFAULT, "ERROR: this is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.\n");
		printf("ERROR: this is a fatal error, exiting now.\n");
		InspIRCd::Exit(EXIT_STATUS_SOCKETENGINE);
	}
	CurrentSetSize = 0;
}

KQueueEngine::~KQueueEngine()
{
	close(EngineHandle);
}

bool KQueueEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	// We always want to read from the socket...
	struct kevent ke;
	EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);

	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		ServerInstance->Log(DEFAULT,"Failed to add fd: %d %s", fd, strerror(errno));
		return false;
	}

	if (!eh->Readable())
	{
		// ...and sometimes want to write
		WantWrite(eh);
	}

	ref[fd] = eh;
	CurrentSetSize++;

	ServerInstance->Log(DEBUG,"New file descriptor: %d", fd);
	return true;
}

bool KQueueEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Log(DEFAULT,"DelFd() on invalid fd: %d", fd);
		return false;
	}

	struct kevent ke;

	// First remove the write filter ignoring errors, since we can't be
	// sure if there are actually any write filters registered.
	EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	// Then remove the read filter.
	EV_SET(&ke, eh->GetFd(), EVFILT_READ, EV_DELETE, 0, 0, NULL);
	int j = kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	if ((j < 0) && !force)
	{
		ServerInstance->Log(DEFAULT,"Failed to remove fd: %d %s",
					  fd, strerror(errno));
		return false;
	}

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Log(DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

void KQueueEngine::WantWrite(EventHandler* eh)
{
	struct kevent ke;
	// EV_ONESHOT since we only ever want one write event
	EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i < 0) {
		ServerInstance->Log(DEFAULT,"Failed to mark for writing: %d %s",
					  eh->GetFd(), strerror(errno));
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
	ts.tv_nsec = 0;
	ts.tv_sec = 1;

	int i = kevent(EngineHandle, NULL, 0, &ke_list[0], MAX_DESCRIPTORS, &ts);

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
		if (ke_list[j].filter == EVFILT_WRITE)
		{
			/* We only ever add write events with EV_ONESHOT, which
			 * means they are automatically removed once such a
			 * event fires, so nothing to do here.
			 */
			if (ref[ke_list[j].ident])
				ref[ke_list[j].ident]->HandleEvent(EVENT_WRITE);
		}
		if (ke_list[j].filter == EVFILT_READ)
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
