/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2012, 2017, 2019, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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

#include <sys/types.h>
#include <sys/event.h>
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

#if defined __NetBSD__ && __NetBSD_Version__ <= 999001400
	inline intptr_t udata_cast(EventHandler* eh)
	{
		// On NetBSD <10 the last parameter of EV_SET is intptr_t.
		return reinterpret_cast<intptr_t>(eh);
	}
#else
	inline void* udata_cast(EventHandler* eh)
	{
		// On other platforms the last parameter of EV_SET is void*.
		return static_cast<void*>(eh);
	}
#endif
}

/** Initialize the kqueue engine
 */
void SocketEngine::Init()
{
	LookupMaxFds();
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
		InitError();
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
	if (!eh->HasFd())
		return false;

	if (!SocketEngine::AddFdRef(eh))
		return false;

	// We always want to read from the socket...
	int fd = eh->GetFd();
	struct kevent* ke = GetChangeKE();
	EV_SET(ke, fd, EVFILT_READ, EV_ADD, 0, 0, udata_cast(eh));

	ServerInstance->Logs.Debug("SOCKET", "New file descriptor: {}", fd);

	eh->SetEventMask(event_mask);
	OnSetEvent(eh, 0, event_mask);
	ResizeDouble(ke_list);

	return true;
}

void SocketEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if (!eh->HasFd())
	{
		ServerInstance->Logs.Debug("SOCKET", "DelFd() on invalid fd: {}", fd);
		return;
	}

	// First remove the write filter ignoring errors, since we can't be
	// sure if there are actually any write filters registered.
	struct kevent* ke = GetChangeKE();
	EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);

	// Then remove the read filter.
	ke = GetChangeKE();
	EV_SET(ke, eh->GetFd(), EVFILT_READ, EV_DELETE, 0, 0, nullptr);

	SocketEngine::DelFdRef(eh);

	ServerInstance->Logs.Debug("SOCKET", "Remove file descriptor: {}", fd);
}

void SocketEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	if ((new_mask & FD_WANT_POLL_WRITE) && !(old_mask & FD_WANT_POLL_WRITE))
	{
		// new poll-style write
		struct kevent* ke = GetChangeKE();
		EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_ADD, 0, 0, udata_cast(eh));
	}
	else if ((old_mask & FD_WANT_POLL_WRITE) && !(new_mask & FD_WANT_POLL_WRITE))
	{
		// removing poll-style write
		struct kevent* ke = GetChangeKE();
		EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
	}
	if ((new_mask & (FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)) && !(old_mask & (FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)))
	{
		struct kevent* ke = GetChangeKE();
		EV_SET(ke, eh->GetFd(), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, udata_cast(eh));
	}
}

int SocketEngine::DispatchEvents()
{
	struct timespec ts;
	ts.tv_nsec = 0;
	ts.tv_sec = 1;

	int i = kevent(EngineHandle, &changelist.front(), ChangePos, &ke_list.front(), static_cast<int>(ke_list.size()), &ts);
	ChangePos = 0;
	ServerInstance->UpdateTime();

	if (i < 0)
		return i;

	stats.TotalEvents += i;

	for (int j = 0; j < i; j++)
	{
		// This can't be a static_cast because udata is intptr_t on NetBSD.
		struct kevent& kev = ke_list[j];
		EventHandler* eh = reinterpret_cast<EventHandler*>(kev.udata);
		if (!eh || !eh->HasFd())
			continue;

		// Copy this in case the vector gets resized and kev invalidated
		const short filter = kev.filter;

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
