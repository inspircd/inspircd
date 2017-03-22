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
#include <iostream>
#include <sys/sysctl.h>

/** A specialisation of the SocketEngine class, designed to use BSD kqueue().
 */
namespace
{
	int EngineHandle;
	unsigned int ChangePos = 0;
	/** These are used by kqueue() to hold socket events
	 */
	std::vector<struct kevent> ke_list(16);

	/** Pending changes
	 */
	std::vector<struct kevent> changelist(8);
}

/** Initialize the kqueue engine
 */
void SocketEngine::Init()
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
	// MAX_DESCRIPTORS is mainly used for display purposes, no problem if the sysctl() below fails
	sysctl(mib, 2, &MAX_DESCRIPTORS, &len, NULL, 0);

	RecoverFromFork();
}

void SocketEngine::RecoverFromFork()
{
	/*
	 * The only bad thing about kqueue is that its fd cant survive a fork and is not inherited.
	 * BUM HATS.
	 *
	 */
	EngineHandle = kqueue();
	if (EngineHandle == -1)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "ERROR: this is a fatal error, exiting now.");
		std::cout << "ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features." << std::endl;
		std::cout << "ERROR: this is a fatal error, exiting now." << std::endl;
		ServerInstance->QuickExit(EXIT_STATUS_SOCKETENGINE);
	}
}

/** Shutdown the kqueue engine
 */
void SocketEngine::Deinit()
{
	Close(EngineHandle);
}

static struct kevent* GetChangeKE()
{
	if (ChangePos >= changelist.size())
		changelist.resize(changelist.size() * 2);
	return &changelist[ChangePos++];
}

bool SocketEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();

	if (fd < 0)
		return false;

	if (!SocketEngine::AddFdRef(eh))
		return false;

	// We always want to read from the socket...
	struct kevent* ke = GetChangeKE();
	EV_SET(ke, fd, EVFILT_READ, EV_ADD, 0, 0, static_cast<void*>(eh));

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "New file descriptor: %d", fd);

	eh->SetEventMask(event_mask);
	OnSetEvent(eh, 0, event_mask);
	ResizeDouble(ke_list);

	return true;
}

void SocketEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	if (fd < 0)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "DelFd() on invalid fd: %d", fd);
		return;
	}

	// First remove the write filter ignoring errors, since we can't be
	// sure if there are actually any write filters registered.
	struct kevent* ke = GetChangeKE();
	EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

	// Then remove the read filter.
	ke = GetChangeKE();
	EV_SET(ke, eh->GetFd(), EVFILT_READ, EV_DELETE, 0, 0, NULL);

	SocketEngine::DelFdRef(eh);

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Remove file descriptor: %d", fd);
}

void SocketEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	if ((new_mask & FD_WANT_POLL_WRITE) && !(old_mask & FD_WANT_POLL_WRITE))
	{
		// new poll-style write
		struct kevent* ke = GetChangeKE();
		EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_ADD, 0, 0, static_cast<void*>(eh));
	}
	else if ((old_mask & FD_WANT_POLL_WRITE) && !(new_mask & FD_WANT_POLL_WRITE))
	{
		// removing poll-style write
		struct kevent* ke = GetChangeKE();
		EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	}
	if ((new_mask & (FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)) && !(old_mask & (FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)))
	{
		struct kevent* ke = GetChangeKE();
		EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, static_cast<void*>(eh));
	}
}

int SocketEngine::DispatchEvents()
{
	struct timespec ts;
	ts.tv_nsec = 0;
	ts.tv_sec = 1;

	int i = kevent(EngineHandle, &changelist.front(), ChangePos, &ke_list.front(), ke_list.size(), &ts);
	ChangePos = 0;
	ServerInstance->UpdateTime();

	if (i < 0)
		return i;

	stats.TotalEvents += i;

	for (int j = 0; j < i; j++)
	{
		struct kevent& kev = ke_list[j];
		EventHandler* eh = static_cast<EventHandler*>(kev.udata);
		if (!eh)
			continue;

		// Copy these in case the vector gets resized and kev invalidated
		const int fd = eh->GetFd();
		const short filter = kev.filter;
		if (fd < 0)
			continue;

		if (kev.flags & EV_EOF)
		{
			stats.ErrorEvents++;
			eh->OnEventHandlerError(kev.fflags);
			continue;
		}
		if (filter == EVFILT_WRITE)
		{
			/* When mask is FD_WANT_FAST_WRITE or FD_WANT_SINGLE_WRITE,
			 * we set a one-shot write, so we need to clear that bit
			 * to detect when it set again.
			 */
			const int bits_to_clr = FD_WANT_SINGLE_WRITE | FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK;
			eh->SetEventMask(eh->GetEventMask() & ~bits_to_clr);
			eh->OnEventHandlerWrite();
		}
		else if (filter == EVFILT_READ)
		{
			eh->SetEventMask(eh->GetEventMask() & ~FD_READ_WILL_BLOCK);
			eh->OnEventHandlerRead();
		}
	}

	return i;
}
