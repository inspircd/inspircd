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
#include "socketengine_poll.h"

PollEngine::PollEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	// Poll requires no special setup (which is nice).
	CurrentSetSize = 0;
	memset(&events, 0, MAX_DESCRIPTORS);
}

PollEngine::~PollEngine()
{
	// No destruction required, either.
}

bool PollEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	ServerInstance->Log(DEBUG, "trying to add fd %d", fd);
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	events[fd].fd = fd;
	if (eh->Readable())
	{
		ServerInstance->Log(DEBUG, "readable");
		events[fd].events = POLLIN;
	}
	else
	{
		ServerInstance->Log(DEBUG, "writable");
		events[fd].events = POLLOUT;
	}

	ServerInstance->Log(DEBUG,"New file descriptor: %d (%d)", fd, events[fd].events);
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
		return false;

	events[fd].fd = -1;
	events[fd].events = 0;

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Log(DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

int PollEngine::GetMaxFds()
{
	return MAX_DESCRIPTORS;
}

int PollEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int PollEngine::DispatchEvents()
{
	int i = poll(events, MAX_DESCRIPTORS, 1000);
	int fd = 0;
	socklen_t codesize = sizeof(int);
	int errcode;
	int processed = 0;

	ServerInstance->Log(DEBUG, "poll returned %d", i);

	if (i > 0)
	{
		for (fd = 0; fd < MAX_DESCRIPTORS && processed != i; fd++)
		{
			if (events[fd].revents)
				processed++;

			ServerInstance->Log(DEBUG, "revents on %d are %d", fd, events[fd].revents);
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

