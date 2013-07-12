/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "exitcodes.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine.h"
#include <iostream>

/** A specialisation of the SocketEngine class, designed to use BSD kqueue().
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
	virtual void DelFd(EventHandler* eh);
	virtual int DispatchEvents();
	virtual std::string GetName();
	virtual void RecoverFromFork();
};

#include <sys/sysctl.h>

KQueueEngine::KQueueEngine()
{
	MAX_DESCRIPTORS = 0;
	int mib[2];
	size_t len;

	mib[0] = CTL_KERN;
#ifdef KERN_MAXFILESPERPROC
	mib[1] = KERN_MAXFILESPERPROC;
#else
	mib[1] = KERN_MAXFILES;
#endif
	len = sizeof(MAX_DESCRIPTORS);
	sysctl(mib, 2, &MAX_DESCRIPTORS, &len, NULL, 0);
	if (MAX_DESCRIPTORS <= 0)
	{
		ServerInstance->Logs->Log("SOCKET", DEFAULT, "ERROR: Can't determine maximum number of open sockets!");
		std::cout << "ERROR: Can't determine maximum number of open sockets!" << std::endl;
		ServerInstance->QuickExit(EXIT_STATUS_SOCKETENGINE);
	}

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
		std::cout << "ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features." << std::endl;
		std::cout << "ERROR: this is a fatal error, exiting now." << std::endl;
		ServerInstance->QuickExit(EXIT_STATUS_SOCKETENGINE);
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

void KQueueEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT,"DelFd() on invalid fd: %d", fd);
		return;
	}

	struct kevent ke;

	// First remove the write filter ignoring errors, since we can't be
	// sure if there are actually any write filters registered.
	EV_SET(&ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	// Then remove the read filter.
	EV_SET(&ke, eh->GetFd(), EVFILT_READ, EV_DELETE, 0, 0, NULL);
	int j = kevent(EngineHandle, &ke, 1, 0, 0, NULL);

	if (j < 0)
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT,"Failed to remove fd: %d %s",
					  fd, strerror(errno));
	}

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
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
	ServerInstance->UpdateTime();

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

			if (eh != ref[ke_list[j].ident])
				// whoops, deleted out from under us
				continue;
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
