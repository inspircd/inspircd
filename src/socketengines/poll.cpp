/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2014, 2017 Adam <Adam@anope.org>
 *   Copyright (C) 2013, 2016-2017, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include <poll.h>
#include <sys/resource.h>

/** A specialisation of the SocketEngine class, designed to use poll().
 */
namespace
{
	/** These are used by poll() to hold socket events
	 */
	std::vector<struct pollfd> events(16);
	/** This vector maps fds to an index in the events array.
	 */
	std::vector<int> fd_mappings(16, -1);
}

void SocketEngine::Init()
{
	LookupMaxFds();
}

void SocketEngine::Deinit()
{
}

void SocketEngine::RecoverFromFork()
{
}

static int mask_to_poll(int event_mask)
{
	int rv = 0;
	if (event_mask & (FD_WANT_POLL_READ | FD_WANT_FAST_READ))
		rv |= POLLIN;
	if (event_mask & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
		rv |= POLLOUT;
	return rv;
}

bool SocketEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();
	if (!eh->HasFd())
	{
		ServerInstance->Logs.Debug("SOCKET", "AddFd out of range: (fd: {})", fd);
		return false;
	}

	if (static_cast<unsigned int>(fd) < fd_mappings.size() && fd_mappings[fd] != -1)
	{
		ServerInstance->Logs.Debug("SOCKET", "Attempt to add duplicate fd: {}", fd);
		return false;
	}

	unsigned int index = static_cast<unsigned int>(CurrentSetSize);

	if (!SocketEngine::AddFdRef(eh))
	{
		ServerInstance->Logs.Debug("SOCKET", "Attempt to add duplicate fd: {}", fd);
		return false;
	}

	while (static_cast<unsigned int>(fd) >= fd_mappings.size())
		fd_mappings.resize(fd_mappings.size() * 2, -1);
	fd_mappings[fd] = index;

	ResizeDouble(events);
	events[index].fd = fd;
	events[index].events = mask_to_poll(event_mask);

	ServerInstance->Logs.Debug("SOCKET", "New file descriptor: {} ({}; index {})", fd, events[index].events, index);
	eh->SetEventMask(event_mask);
	return true;
}

void SocketEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	int fd = eh->GetFd();
	if (!eh->HasFd() || static_cast<unsigned int>(fd) >= fd_mappings.size() || fd_mappings[fd] == -1)
	{
		ServerInstance->Logs.Debug("SOCKET", "SetEvents() on unknown fd: {}", eh->GetFd());
		return;
	}

	events[fd_mappings[fd]].events = mask_to_poll(new_mask);
}

void SocketEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if (!eh->HasFd())
	{
		ServerInstance->Logs.Debug("SOCKET", "DelFd out of range: (fd: {})", fd);
		return;
	}

	if (static_cast<unsigned int>(fd) >= fd_mappings.size() || fd_mappings[fd] == -1)
	{
		ServerInstance->Logs.Debug("SOCKET", "DelFd() on unknown fd: {}", fd);
		return;
	}

	unsigned int index = fd_mappings[fd];
	unsigned int last_index = static_cast<unsigned int>(CurrentSetSize - 1);
	int last_fd = events[last_index].fd;

	if (index != last_index)
	{
		// We need to move the last fd we got into this gap (gaps are evil!)

		// So update the mapping for the last fd to its new position
		fd_mappings[last_fd] = index;

		// move last_fd from last_index into index
		events[index].fd = last_fd;
		events[index].events = events[last_index].events;
	}

	// Now remove all data for the last fd we got into out list.
	// Above code made sure this always is right
	fd_mappings[fd] = -1;
	events[last_index].fd = 0;
	events[last_index].events = 0;

	SocketEngine::DelFdRef(eh);

	ServerInstance->Logs.Debug("SOCKET", "Remove file descriptor: {} (index: {}) "
			"(Filled gap with: {} (index: {}))", fd, index, last_fd, last_index);
}

int SocketEngine::DispatchEvents()
{
	int i = poll(&events[0], static_cast<unsigned int>(CurrentSetSize), 1000);
	int processed = 0;
	ServerInstance->UpdateTime();

	for (size_t index = 0; index < CurrentSetSize && processed < i; index++)
	{
		struct pollfd& pfd = events[index];

		// Copy these in case the vector gets resized and pfd invalidated
		const int fd = pfd.fd;
		const short revents = pfd.revents;

		if (revents)
			processed++;

		EventHandler* eh = GetRef(fd);
		if (!eh)
			continue;

		if (revents & POLLHUP)
		{
			eh->OnEventHandlerError(0);
			continue;
		}

		if (revents & POLLERR)
		{
			// Get error number
			socklen_t codesize = sizeof(int);
			int errcode;
			if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
				errcode = errno;
			eh->OnEventHandlerError(errcode);
			continue;
		}

		if (revents & POLLIN)
		{
			eh->SetEventMask(eh->GetEventMask() & ~FD_READ_WILL_BLOCK);
			eh->OnEventHandlerRead();
			if (eh != GetRef(fd))
				// whoops, deleted out from under us
				continue;
		}

		if (revents & POLLOUT)
		{
			int mask = eh->GetEventMask();
			mask &= ~(FD_WRITE_WILL_BLOCK | FD_WANT_SINGLE_WRITE);
			eh->SetEventMask(mask);

			// The vector could've been resized, reference can be invalid by now; don't use it
			events[index].events = mask_to_poll(mask);
			eh->OnEventHandlerWrite();
		}
	}

	return i;
}
