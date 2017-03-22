/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
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

#ifndef _WIN32
#include <sys/select.h>
#endif // _WIN32

/** A specialisation of the SocketEngine class, designed to use traditional select().
 */
namespace
{
	fd_set ReadSet, WriteSet, ErrSet;
	int MaxFD = 0;
}

void SocketEngine::Init()
{
	MAX_DESCRIPTORS = FD_SETSIZE;

	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);
	FD_ZERO(&ErrSet);
}

void SocketEngine::Deinit()
{
}

void SocketEngine::RecoverFromFork()
{
}

bool SocketEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (!SocketEngine::AddFdRef(eh))
		return false;

	eh->SetEventMask(event_mask);
	OnSetEvent(eh, 0, event_mask);
	FD_SET(fd, &ErrSet);
	if (fd > MaxFD)
		MaxFD = fd;

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "New file descriptor: %d", fd);
	return true;
}

void SocketEngine::DelFd(EventHandler* eh)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return;

	SocketEngine::DelFdRef(eh);

	FD_CLR(fd, &ReadSet);
	FD_CLR(fd, &WriteSet);
	FD_CLR(fd, &ErrSet);
	if (fd == MaxFD)
		--MaxFD;

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Remove file descriptor: %d", fd);
}

void SocketEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	int fd = eh->GetFd();
	int diff = old_mask ^ new_mask;

	if (diff & (FD_WANT_POLL_READ | FD_WANT_FAST_READ))
	{
		if (new_mask & (FD_WANT_POLL_READ | FD_WANT_FAST_READ))
			FD_SET(fd, &ReadSet);
		else
			FD_CLR(fd, &ReadSet);
	}
	if (diff & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
	{
		if (new_mask & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
			FD_SET(fd, &WriteSet);
		else
			FD_CLR(fd, &WriteSet);
	}
}

int SocketEngine::DispatchEvents()
{
	timeval tval;
	tval.tv_sec = 1;
	tval.tv_usec = 0;

	fd_set rfdset = ReadSet, wfdset = WriteSet, errfdset = ErrSet;

	int sresult = select(MaxFD + 1, &rfdset, &wfdset, &errfdset, &tval);
	ServerInstance->UpdateTime();

	for (int i = 0, j = sresult; i <= MaxFD && j > 0; i++)
	{
		int has_read = FD_ISSET(i, &rfdset), has_write = FD_ISSET(i, &wfdset), has_error = FD_ISSET(i, &errfdset);

		if (!(has_read || has_write || has_error))
			continue;

		--j;

		EventHandler* ev = GetRef(i);
		if (!ev)
			continue;

		if (has_error)
		{
			stats.ErrorEvents++;

			socklen_t codesize = sizeof(int);
			int errcode = 0;
			if (getsockopt(i, SOL_SOCKET, SO_ERROR, (char*)&errcode, &codesize) < 0)
				errcode = errno;

			ev->OnEventHandlerError(errcode);
			continue;
		}

		if (has_read)
		{
			ev->SetEventMask(ev->GetEventMask() & ~FD_READ_WILL_BLOCK);
			ev->OnEventHandlerRead();
			if (ev != GetRef(i))
				continue;
		}

		if (has_write)
		{
			int newmask = (ev->GetEventMask() & ~(FD_WRITE_WILL_BLOCK | FD_WANT_SINGLE_WRITE));
			SocketEngine::OnSetEvent(ev, ev->GetEventMask(), newmask);
			ev->SetEventMask(newmask);
			ev->OnEventHandlerWrite();
		}
	}

	return sresult;
}
