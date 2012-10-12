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


#include "inspircd_config.h"

#include "inspircd.h"
#include "socketengine.h"

#ifndef _WIN32
#include <sys/select.h>
#endif // _WIN32

/** A specialisation of the SocketEngine class, designed to use traditional select().
 */
class SelectEngine : public SocketEngine
{
public:
	/** Create a new SelectEngine
	 */
	SelectEngine();
	/** Delete a SelectEngine
	 */
	virtual ~SelectEngine();
	virtual bool AddFd(EventHandler* eh, int event_mask);
	virtual void DelFd(EventHandler* eh);
	void OnSetEvent(EventHandler* eh, int, int);
	virtual int DispatchEvents();
	virtual std::string GetName();
};

SelectEngine::SelectEngine()
{
	MAX_DESCRIPTORS = FD_SETSIZE;
	CurrentSetSize = 0;

	ref = new EventHandler* [GetMaxFds()];
	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
}

SelectEngine::~SelectEngine()
{
	delete[] ref;
}

bool SelectEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	SocketEngine::SetEventMask(eh, event_mask);
	CurrentSetSize++;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);
	return true;
}

void SelectEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return;

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
}

void SelectEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	// deal with it later
}

int SelectEngine::DispatchEvents()
{
	timeval tval;
	int sresult = 0;
	socklen_t codesize = sizeof(int);
	int errcode = 0;

	fd_set wfdset, rfdset, errfdset;
	FD_ZERO(&wfdset);
	FD_ZERO(&rfdset);
	FD_ZERO(&errfdset);

	/* Populate the select FD sets (this is why select sucks compared to epoll, kqueue) */
	for (unsigned int i = 0; i < FD_SETSIZE; i++)
	{
		EventHandler* eh = ref[i];
		if (!eh)
			continue;
		int state = eh->GetEventMask();
		if (state & (FD_WANT_POLL_READ | FD_WANT_FAST_READ))
			FD_SET (i, &rfdset);
		if (state & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
			FD_SET (i, &wfdset);
		FD_SET (i, &errfdset);
	}

	/* One second wait */
	tval.tv_sec = 1;
	tval.tv_usec = 0;

	sresult = select(FD_SETSIZE, &rfdset, &wfdset, &errfdset, &tval);
	ServerInstance->UpdateTime();

	/* Nothing to process this time around */
	if (sresult < 1)
		return 0;

	for (int i = 0; i < FD_SETSIZE; i++)
	{
		EventHandler* ev = ref[i];
		if (ev)
		{
			if (FD_ISSET (i, &errfdset))
			{
				ErrorEvents++;
				if (getsockopt(i, SOL_SOCKET, SO_ERROR, (char*)&errcode, &codesize) < 0)
					errcode = errno;

				ev->HandleEvent(EVENT_ERROR, errcode);
				continue;
			}
			else
			{
				/* NOTE: This is a pair of seperate if statements as the socket
				 * may be in both read and writeable state at the same time.
				 * If an error event occurs above it is not worth processing the
				 * read and write states even if set.
				 */
				if (FD_ISSET (i, &rfdset))
				{
					ReadEvents++;
					SetEventMask(ev, ev->GetEventMask() & ~FD_READ_WILL_BLOCK);
					ev->HandleEvent(EVENT_READ);
					if (ev != ref[i])
						continue;
				}
				if (FD_ISSET (i, &wfdset))
				{
					WriteEvents++;
					SetEventMask(ev, ev->GetEventMask() & ~(FD_WRITE_WILL_BLOCK | FD_WANT_SINGLE_WRITE));
					ev->HandleEvent(EVENT_WRITE);
				}
			}
		}
	}

	return sresult;
}

std::string SelectEngine::GetName()
{
	return "select";
}

SocketEngine* CreateSocketEngine()
{
	return new SelectEngine;
}
