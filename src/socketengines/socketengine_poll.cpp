/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#include "inspircd.h"
#include "exitcodes.h"

#ifndef SOCKETENGINE_POLL
#define SOCKETENGINE_POLL

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
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
	struct pollfd *events;
	/** This map maps fds to an index in the events array.
	 */
	std::map<int, unsigned int> fd_mappings;
public:
	/** Create a new PollEngine
	 */
	PollEngine();
	/** Delete a PollEngine
	 */
	virtual ~PollEngine();
	virtual bool AddFd(EventHandler* eh, int event_mask);
	virtual void OnSetEvent(EventHandler* eh, int old_mask, int new_mask);
	virtual EventHandler* GetRef(int fd);
	virtual void DelFd(EventHandler* eh);
	virtual int DispatchEvents();
	virtual std::string GetName();
};

#endif

PollEngine::PollEngine()
{
	CurrentSetSize = 0;
	struct rlimit limits;
	if (!getrlimit(RLIMIT_NOFILE, &limits))
	{
		MAX_DESCRIPTORS = limits.rlim_cur;
	}
	else
	{
		ServerInstance->Logs->Log("SOCKET", DEFAULT, "ERROR: Can't determine maximum number of open sockets: %s", strerror(errno));
		std::cout << "ERROR: Can't determine maximum number of open sockets: " << strerror(errno) << std::endl;
		ServerInstance->QuickExit(EXIT_STATUS_SOCKETENGINE);
	}

	ref = new EventHandler* [GetMaxFds()];
	events = new struct pollfd[GetMaxFds()];

	memset(events, 0, GetMaxFds() * sizeof(struct pollfd));
	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
}

PollEngine::~PollEngine()
{
	// No destruction required, either.
	delete[] ref;
	delete[] events;
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
		ServerInstance->Logs->Log("SOCKET",DEBUG,"AddFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	if (fd_mappings.find(fd) != fd_mappings.end())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Attempt to add duplicate fd: %d", fd);
		return false;
	}

	unsigned int index = CurrentSetSize;

	fd_mappings[fd] = index;
	ref[index] = eh;
	events[index].fd = fd;
	events[index].events = mask_to_poll(event_mask);

	ServerInstance->Logs->Log("SOCKET", DEBUG,"New file descriptor: %d (%d; index %d)", fd, events[index].events, index);
	SocketEngine::SetEventMask(eh, event_mask);
	CurrentSetSize++;
	return true;
}

EventHandler* PollEngine::GetRef(int fd)
{
	std::map<int, unsigned int>::iterator it = fd_mappings.find(fd);
	if (it == fd_mappings.end())
		return NULL;
	return ref[it->second];
}

void PollEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	std::map<int, unsigned int>::iterator it = fd_mappings.find(eh->GetFd());
	if (it == fd_mappings.end())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"SetEvents() on unknown fd: %d", eh->GetFd());
		return;
	}

	events[it->second].events = mask_to_poll(new_mask);
}

void PollEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "DelFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return;
	}

	std::map<int, unsigned int>::iterator it = fd_mappings.find(fd);
	if (it == fd_mappings.end())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"DelFd() on unknown fd: %d", fd);
		return;
	}

	unsigned int index = it->second;
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

		ref[index] = ref[last_index];
	}

	// Now remove all data for the last fd we got into out list.
	// Above code made sure this always is right
	fd_mappings.erase(it);
	events[last_index].fd = 0;
	events[last_index].events = 0;
	ref[last_index] = NULL;

	CurrentSetSize--;

	ServerInstance->Logs->Log("SOCKET", DEBUG, "Remove file descriptor: %d (index: %d) "
			"(Filled gap with: %d (index: %d))", fd, index, last_fd, last_index);
}

int PollEngine::DispatchEvents()
{
	int i = poll(events, CurrentSetSize, 1000);
	int index;
	socklen_t codesize = sizeof(int);
	int errcode;
	int processed = 0;
	ServerInstance->UpdateTime();

	if (i > 0)
	{
		for (index = 0; index < CurrentSetSize && processed != i; index++)
		{
			if (events[index].revents)
				processed++;
			EventHandler* eh = ref[index];
			if (!eh)
				continue;

			if (events[index].revents & POLLHUP)
			{
				eh->HandleEvent(EVENT_ERROR, 0);
				continue;
			}

			if (events[index].revents & POLLERR)
			{
				// Get fd
				int fd = events[index].fd;

				// Get error number
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
					errcode = errno;
				eh->HandleEvent(EVENT_ERROR, errcode);
				continue;
			}

			if (events[index].revents & POLLIN)
			{
				SetEventMask(eh, eh->GetEventMask() & ~FD_READ_WILL_BLOCK);
				eh->HandleEvent(EVENT_READ);
				if (eh != ref[index])
					// whoops, deleted out from under us
					continue;
			}
			
			if (events[index].revents & POLLOUT)
			{
				int mask = eh->GetEventMask();
				mask &= ~(FD_WRITE_WILL_BLOCK | FD_WANT_SINGLE_WRITE);
				SetEventMask(eh, mask);
				events[index].events = mask_to_poll(mask);
				eh->HandleEvent(EVENT_WRITE);
			}
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
