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
#ifndef WINDOWS
#include <sys/select.h>
#endif // WINDOWS
#include "socketengines/socketengine_select.h"


SelectEngine::SelectEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	MAX_DESCRIPTORS = FD_SETSIZE;
	EngineHandle = 0;
	CurrentSetSize = 0;

	writeable.assign(GetMaxFds(), false);
	ref = new EventHandler* [GetMaxFds()];
	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
}

SelectEngine::~SelectEngine()
{
	delete[] ref;
}

bool SelectEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	fds.insert(fd);
	ref[fd] = eh;
	CurrentSetSize++;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);
	return true;
}

void SelectEngine::WantWrite(EventHandler* eh)
{
	writeable[eh->GetFd()] = true;
}

bool SelectEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	std::set<int>::iterator t = fds.find(fd);
	if (t != fds.end())
		fds.erase(t);

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

int SelectEngine::GetMaxFds()
{
	return FD_SETSIZE;
}

int SelectEngine::GetRemainingFds()
{
	return GetMaxFds() - CurrentSetSize;
}

int SelectEngine::DispatchEvents()
{
	timeval tval;
	int sresult = 0;
	socklen_t codesize = sizeof(int);
	int errcode = 0;

	FD_ZERO(&wfdset);
	FD_ZERO(&rfdset);
	FD_ZERO(&errfdset);

	/* Populate the select FD set (this is why select sucks compared to epoll, kqueue, IOCP) */
	for (std::set<int>::iterator a = fds.begin(); a != fds.end(); a++)
	{
		if (ref[*a]->Readable())
			/* Read notifications */
			FD_SET (*a, &rfdset);
		else
			/* Write notifications */
			FD_SET (*a, &wfdset);

		/* Explicitly one-time writeable */
		if (writeable[*a])
			FD_SET (*a, &wfdset);

		/* All sockets must receive error notifications regardless */
		FD_SET (*a, &errfdset);
	}

	/* One second waits */
	tval.tv_sec = 1;
	tval.tv_usec = 0;

	sresult = select(FD_SETSIZE, &rfdset, &wfdset, &errfdset, &tval);

	/* Nothing to process this time around */
	if (sresult < 1)
		return 0;

	std::vector<int> copy(fds.begin(), fds.end());
	for (std::vector<int>::iterator a = copy.begin(); a != copy.end(); a++)
	{
		EventHandler* ev = ref[*a];
		if (ev)
		{
			if (FD_ISSET (ev->GetFd(), &errfdset))
			{
				ErrorEvents++;
				if (getsockopt(ev->GetFd(), SOL_SOCKET, SO_ERROR, (char*)&errcode, &codesize) < 0)
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
				if (FD_ISSET (ev->GetFd(), &wfdset))
				{
					WriteEvents++;
					writeable[ev->GetFd()] = false;
					ev->HandleEvent(EVENT_WRITE);
				}
				if (FD_ISSET (ev->GetFd(), &rfdset))
				{
						ReadEvents++;
						ev->HandleEvent(EVENT_READ);
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
