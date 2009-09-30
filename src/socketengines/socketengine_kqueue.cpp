/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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

#ifndef __SOCKETENGINE_KQUEUE__
#define __SOCKETENGINE_KQUEUE__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine.h"

/** A specialisation of the SocketEngine class, designed to use FreeBSD kqueue().
 */
class KQueueEngine : public SocketEngine
{
private:
	int EngineHandle;
	/** These are used by kqueue() to hold socket events
	 */
	struct kevent* ke_list;
	/** This is a specialised time value used by kqueue()
	 */
	struct timespec ts;
public:
	/** Create a new KQueueEngine
	 */
	KQueueEngine();
	/** Delete a KQueueEngine
	 */
	virtual ~KQueueEngine();
	bool AddFd(EventHandler* eh, int event_mask);
	void OnSetEvent(EventHandler* eh, int old_mask, int new_mask);
	virtual bool DelFd(EventHandler* eh, bool force = false);
	virtual int DispatchEvents();
	virtual std::string GetName();
	virtual void RecoverFromFork();
};

#endif

#include <sys/sysctl.h>

KQueueEngine::KQueueEngine()
{
	MAX_DESCRIPTORS = 0;
	this->RecoverFromFork();
	ke_list = new struct kevent[GetMaxFds()];
	ref = new EventHandler* [GetMaxFds()];
	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
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
}

KQueueEngine::~KQueueEngine()
{
	this->Close(EngineHandle);
	delete[] ref;
	delete[] ke_list;
}

bool KQueueEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (ref[fd])
		return false;

	// We always want to read from the socket...
	struct kevent ke;
	EV_SET(&ke, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);

	int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
	if (i == -1)
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT,"Failed to add fd: %d %s",
					  fd, strerror(errno));
		return false;
	}

	ref[fd] = eh;
	SocketEngine::SetEventMask(eh, event_mask);
	OnSetEvent(eh, 0, event_mask);
	CurrentSetSize++;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);
	return true;
}

bool KQueueEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT,"DelFd() on invalid fd: %d", fd);
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
		ServerInstance->Logs->Log("SOCKET",DEFAULT,"Failed to remove fd: %d %s",
					  fd, strerror(errno));
		return false;
	}

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

void KQueueEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	if ((new_mask & FD_WANT_POLL_WRITE) && !(old_mask & FD_WANT_POLL_WRITE))
	{
		// new poll-style write
		struct kevent ke;
		EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_ADD, 0, 0, NULL);
		int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
		if (i < 0) {
			ServerInstance->Logs->Log("SOCKET",DEFAULT,"Failed to mark for writing: %d %s",
						  eh->GetFd(), strerror(errno));
		}
	}
	else if ((old_mask & FD_WANT_POLL_WRITE) && !(new_mask & FD_WANT_POLL_WRITE))
	{
		// removing poll-style write
		struct kevent ke;
		EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
		if (i < 0) {
			ServerInstance->Logs->Log("SOCKET",DEFAULT,"Failed to mark for writing: %d %s",
						  eh->GetFd(), strerror(errno));
		}
	}
	if ((new_mask & (FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)) && !(old_mask & (FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)))
	{
		// new one-shot write
		struct kevent ke;
		EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, NULL);
		int i = kevent(EngineHandle, &ke, 1, 0, 0, NULL);
		if (i < 0) {
			ServerInstance->Logs->Log("SOCKET",DEFAULT,"Failed to mark for writing: %d %s",
						  eh->GetFd(), strerror(errno));
		}
	}
}

int KQueueEngine::DispatchEvents()
{
	ts.tv_nsec = 0;
	ts.tv_sec = 1;

	int i = kevent(EngineHandle, NULL, 0, &ke_list[0], GetMaxFds(), &ts);

	TotalEvents += i;

	for (int j = 0; j < i; j++)
	{
		EventHandler* eh = ref[ke_list[j].ident];
		if (!eh)
			continue;
		if (ke_list[j].flags & EV_EOF)
		{
			ErrorEvents++;
			eh->HandleEvent(EVENT_ERROR, ke_list[j].fflags);
			continue;
		}
		if (ke_list[j].filter == EVFILT_WRITE)
		{
			WriteEvents++;
			/* When mask is FD_WANT_FAST_WRITE or FD_WANT_SINGLE_WRITE,
			 * we set a one-shot write, so we need to clear that bit
			 * to detect when it set again.
			 */
			const int bits_to_clr = FD_WANT_SINGLE_WRITE | FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK;
			SetEventMask(eh, eh->GetEventMask() & ~bits_to_clr);
			eh->HandleEvent(EVENT_WRITE);
		}
		if (ke_list[j].filter == EVFILT_READ)
		{
			ReadEvents++;
			SetEventMask(eh, eh->GetEventMask() & ~FD_READ_WILL_BLOCK);
			eh->HandleEvent(EVENT_READ);
		}
	}

	return i;
}

std::string KQueueEngine::GetName()
{
	return "kqueue";
}

SocketEngine* CreateSocketEngine()
{
	return new KQueueEngine;
}
