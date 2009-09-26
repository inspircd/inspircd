/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#ifndef WINDOWS
#include <sys/select.h>
#endif // WINDOWS
#include "socketengines/socketengine_select.h"


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

bool SelectEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();

	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
	return true;
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

	/* Populate the select FD sets (this is why select sucks compared to epoll, kqueue, IOCP) */
	for (int i = 0; i < FD_SETSIZE; i++)
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
