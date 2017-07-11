/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

#ifndef __sun
# error You need Solaris 10 or later to make use of this code.
#endif

#include "inspircd.h"
#include <port.h>
#include <iostream>
#include <ulimit.h>

/** A specialisation of the SocketEngine class, designed to use solaris 10 I/O completion ports
 */
namespace
{
	/** These are used by ports to hold socket events
	 */
	std::vector<port_event_t> events(16);
	int EngineHandle;
}

/** Initialize ports engine
 */
void SocketEngine::Init()
{
	// MAX_DESCRIPTORS is mainly used for display purposes, no problem if ulimit() fails and returns a negative number
	MAX_DESCRIPTORS = ulimit(4, 0);

	EngineHandle = port_create();

	if (EngineHandle == -1)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_SPARSE, "ERROR: Could not initialize socket engine: %s", strerror(errno));
		ServerInstance->Logs->Log("SOCKET", LOG_SPARSE, "ERROR: This is a fatal error, exiting now.");
		std::cout << "ERROR: Could not initialize socket engine: " << strerror(errno) << std::endl;
		std::cout << "ERROR: This is a fatal error, exiting now." << std::endl;
		ServerInstance->QuickExit(EXIT_STATUS_SOCKETENGINE);
	}
}

/** Shutdown the ports engine
 */
void SocketEngine::Deinit()
{
	SocketEngine::Close(EngineHandle);
}

void SocketEngine::RecoverFromFork()
{
}

static int mask_to_events(int event_mask)
{
	int rv = 0;
	if (event_mask & (FD_WANT_POLL_READ | FD_WANT_FAST_READ))
		rv |= POLLRDNORM;
	if (event_mask & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
		rv |= POLLWRNORM;
	return rv;
}

bool SocketEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();
	if (fd < 0)
		return false;

	if (!SocketEngine::AddFdRef(eh))
		return false;

	eh->SetEventMask(event_mask);
	port_associate(EngineHandle, PORT_SOURCE_FD, fd, mask_to_events(event_mask), eh);

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "New file descriptor: %d", fd);
	ResizeDouble(events);

	return true;
}

void SocketEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	if (mask_to_events(new_mask) != mask_to_events(old_mask))
		port_associate(EngineHandle, PORT_SOURCE_FD, eh->GetFd(), mask_to_events(new_mask), eh);
}

void SocketEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if (fd < 0)
		return;

	port_dissociate(EngineHandle, PORT_SOURCE_FD, fd);

	SocketEngine::DelFdRef(eh);

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Remove file descriptor: %d", fd);
}

int SocketEngine::DispatchEvents()
{
	struct timespec poll_time;

	poll_time.tv_sec = 1;
	poll_time.tv_nsec = 0;

	unsigned int nget = 1; // used to denote a retrieve request.
	int ret = port_getn(EngineHandle, &events[0], events.size(), &nget, &poll_time);
	ServerInstance->UpdateTime();

	// first handle an error condition
	if (ret == -1)
		return -1;

	stats.TotalEvents += nget;

	unsigned int i;
	for (i = 0; i < nget; i++)
	{
		port_event_t& ev = events[i];

		if (ev.portev_source != PORT_SOURCE_FD)
			continue;

		// Copy these in case the vector gets resized and ev invalidated
		const int fd = ev.portev_object;
		const int portev_events = ev.portev_events;
		EventHandler* eh = static_cast<EventHandler*>(ev.portev_user);
		if (eh->GetFd() < 0)
			continue;

		int mask = eh->GetEventMask();
		if (portev_events & POLLWRNORM)
			mask &= ~(FD_WRITE_WILL_BLOCK | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE);
		if (portev_events & POLLRDNORM)
			mask &= ~FD_READ_WILL_BLOCK;
		// reinsert port for next time around, pretending to be one-shot for writes
		eh->SetEventMask(mask);
		port_associate(EngineHandle, PORT_SOURCE_FD, fd, mask_to_events(mask), eh);
		if (portev_events & POLLRDNORM)
		{
			eh->OnEventHandlerRead();
			if (eh != GetRef(fd))
				continue;
		}
		if (portev_events & POLLWRNORM)
		{
			eh->OnEventHandlerWrite();
		}
	}

	return (int)i;
}
