/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef SOCKETENGINE_POLL
#define SOCKETENGINE_POLL

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include "exitcodes.h"
#include "inspircd.h"
#include "socketengine.h"

#ifndef _WIN32
# ifndef __USE_XOPEN
#  define __USE_XOPEN /* fuck every fucking OS ever made. needed by poll.h to work.*/
# endif
# include <poll.h>
# include <sys/poll.h>
# include <sys/resource.h>
#else
# define struct pollfd WSAPOLLFD
# define poll WSAPoll
#endif

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use poll().
 */
class PollEngine : public SocketEngine
{
private:
	/** These are used by poll() to hold socket events
	 */
	std::vector<struct pollfd> events;
	/** This vector maps fds to an index in the events array.
	 */
	std::vector<int> fd_mappings;
public:
	/** Create a new PollEngine
	 */
	PollEngine();
	virtual bool AddFd(EventHandler* eh, int event_mask);
	virtual void OnSetEvent(EventHandler* eh, int old_mask, int new_mask);
	virtual void DelFd(EventHandler* eh);
	virtual int DispatchEvents();
	virtual std::string GetName();
};

#endif

PollEngine::PollEngine() : events(1), fd_mappings(1)
{
	CurrentSetSize = 0;
	struct rlimit limits;
	if (!getrlimit(RLIMIT_NOFILE, &limits))
	{
		MAX_DESCRIPTORS = limits.rlim_cur;
	}
	else
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "ERROR: Can't determine maximum number of open sockets: %s", strerror(errno));
		std::cout << "ERROR: Can't determine maximum number of open sockets: " << strerror(errno) << std::endl;
		ServerInstance->QuickExit(EXIT_STATUS_SOCKETENGINE);
	}
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

bool PollEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "AddFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	if (static_cast<unsigned int>(fd) < fd_mappings.size() && fd_mappings[fd] != -1)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Attempt to add duplicate fd: %d", fd);
		return false;
	}

	if (!SocketEngine::AddFd(eh))
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Attempt to add duplicate fd: %d", fd);
		return false;
	}

	unsigned int index = CurrentSetSize;

	while (static_cast<unsigned int>(fd) >= fd_mappings.size())
		fd_mappings.resize(fd_mappings.size() * 2, -1);
	fd_mappings[fd] = index;

	ResizeDouble(events);
	events[index].fd = fd;
	events[index].events = mask_to_poll(event_mask);

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "New file descriptor: %d (%d; index %d)", fd, events[index].events, index);
	SocketEngine::SetEventMask(eh, event_mask);
	CurrentSetSize++;
	return true;
}

void PollEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	int fd = eh->GetFd();
	if (fd < 0 || static_cast<unsigned int>(fd) >= fd_mappings.size() || fd_mappings[fd] == -1)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "SetEvents() on unknown fd: %d", eh->GetFd());
		return;
	}

	events[fd_mappings[fd]].events = mask_to_poll(new_mask);
}

void PollEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "DelFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return;
	}

	if (static_cast<unsigned int>(fd) >= fd_mappings.size() || fd_mappings[fd] == -1)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "DelFd() on unknown fd: %d", fd);
		return;
	}

	unsigned int index = fd_mappings[fd];
	unsigned int last_index = CurrentSetSize - 1;
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

	SocketEngine::DelFd(eh);

	CurrentSetSize--;

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Remove file descriptor: %d (index: %d) "
			"(Filled gap with: %d (index: %d))", fd, index, last_fd, last_index);
}

int PollEngine::DispatchEvents()
{
	int i = poll(&events[0], CurrentSetSize, 1000);
	int index;
	socklen_t codesize = sizeof(int);
	int errcode;
	int processed = 0;
	ServerInstance->UpdateTime();

	for (index = 0; index < CurrentSetSize && processed < i; index++)
	{
		struct pollfd& pfd = events[index];

		if (pfd.revents)
			processed++;

		EventHandler* eh = GetRef(pfd.fd);
		if (!eh)
			continue;

		if (pfd.revents & POLLHUP)
		{
			eh->HandleEvent(EVENT_ERROR, 0);
			continue;
		}

		if (pfd.revents & POLLERR)
		{
			// Get error number
			if (getsockopt(pfd.fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
				errcode = errno;
			eh->HandleEvent(EVENT_ERROR, errcode);
			continue;
		}

		if (pfd.revents & POLLIN)
		{
			SetEventMask(eh, eh->GetEventMask() & ~FD_READ_WILL_BLOCK);
			eh->HandleEvent(EVENT_READ);
			if (eh != GetRef(pfd.fd))
				// whoops, deleted out from under us
				continue;
		}

		if (pfd.revents & POLLOUT)
		{
			int mask = eh->GetEventMask();
			mask &= ~(FD_WRITE_WILL_BLOCK | FD_WANT_SINGLE_WRITE);
			SetEventMask(eh, mask);
			pfd.events = mask_to_poll(mask);
			eh->HandleEvent(EVENT_WRITE);
		}
	}

	return i;
}

std::string PollEngine::GetName()
{
	return "poll";
}

SocketEngine* CreateSocketEngine()
{
	return new PollEngine;
}
