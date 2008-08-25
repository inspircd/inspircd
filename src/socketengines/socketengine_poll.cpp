/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include "socketengines/socketengine_poll.h"
#include <poll.h>
#include <ulimit.h>

PollEngine::PollEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	// Poll requires no special setup (which is nice).
	CurrentSetSize = 0;
	MAX_DESCRIPTORS = 0;

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

bool PollEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"AddFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	if (GetRemainingFds() <= 1)
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"No remaining FDs cannot add fd: %d", fd);
		return false;
	}

	if (ref[fd])
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Attempt to add duplicate fd: %d", fd);
		return false;
	}

	ref[fd] = eh;
	events[fd].fd = fd;
	if (eh->Readable())
	{
		events[fd].events = POLLIN;
	}
	else
	{
		events[fd].events = POLLOUT;
	}

	ServerInstance->Logs->Log("SOCKET", DEBUG,"New file descriptor: %d (%d)", fd, events[fd].events);
	CurrentSetSize++;
	return true;
}

void PollEngine::WantWrite(EventHandler* eh)
{
	events[eh->GetFd()].events = POLLIN | POLLOUT;
}

bool PollEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "DelFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	events[fd].fd = -1;
	events[fd].events = 0;

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET", DEBUG, "Remove file descriptor: %d", fd);
	return true;
}

int PollEngine::GetMaxFds()
{
	if (MAX_DESCRIPTORS)
		return MAX_DESCRIPTORS;

	int max = ulimit(4, 0);
	if (max > 0)
	{
		MAX_DESCRIPTORS = max;
		return max;
	}
	else
	{
		ServerInstance->Logs->Log("SOCKET", DEFAULT, "ERROR: Can't determine maximum number of open sockets!");
		printf("ERROR: Can't determine maximum number of open sockets!\n");
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}
	return 0;
}

int PollEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int PollEngine::DispatchEvents()
{
	int i = poll(events, GetMaxFds() - 1, 1000);
	int fd = 0;
	socklen_t codesize = sizeof(int);
	int errcode;
	int processed = 0;

	if (i > 0)
	{
		for (fd = 0; fd < GetMaxFds() - 1 && processed != i; fd++)
		{
			if (events[fd].revents)
				processed++;

			if (events[fd].revents & POLLHUP)
			{
				if (ref[fd])
					ref[fd]->HandleEvent(EVENT_ERROR, 0);
				continue;
			}

			if (events[fd].revents & POLLERR)
			{
				// Get error number
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
					errcode = errno;
				if (ref[fd])
					ref[fd]->HandleEvent(EVENT_ERROR, errcode);
				continue;
			}

			if (events[fd].revents & POLLOUT)
			{
				// Switch to wanting read again
				// event handlers have to request to write again if they need it
				events[fd].events = POLLIN;


				if (ref[fd])
					ref[fd]->HandleEvent(EVENT_WRITE);
			}

			if (events[fd].revents & POLLIN)
			{
				if (ref[fd])
					ref[fd]->HandleEvent(EVENT_READ);
			}
		}
	}

	return i;
}

std::string PollEngine::GetName()
{
	return "poll";
}

