/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include "socketengines/socketengine_poll.h"
#include <ulimit.h>
#ifdef __FreeBSD__
	#include <sys/sysctl.h>
#endif

PollEngine::PollEngine()
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

bool PollEngine::AddFd(EventHandler* eh, bool writeFirst)
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

	if (fd_mappings.find(fd) != fd_mappings.end())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Attempt to add duplicate fd: %d", fd);
		return false;
	}

	unsigned int index = CurrentSetSize;

	fd_mappings[fd] = index;
	ref[index] = eh;
	events[index].fd = fd;
	if (writeFirst)
	{
		events[index].events = POLLOUT;
	}
	else
	{
		events[index].events = POLLIN;
	}

	ServerInstance->Logs->Log("SOCKET", DEBUG,"New file descriptor: %d (%d; index %d)", fd, events[fd].events, index);
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

void PollEngine::WantWrite(EventHandler* eh)
{
	std::map<int, unsigned int>::iterator it = fd_mappings.find(eh->GetFd());
	if (it == fd_mappings.end())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"WantWrite() on unknown fd: %d", eh->GetFd());
		return;
	}

	events[it->second].events = POLLIN | POLLOUT;
}

bool PollEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "DelFd out of range: (fd: %d, max: %d)", fd, GetMaxFds());
		return false;
	}

	std::map<int, unsigned int>::iterator it = fd_mappings.find(fd);
	if (it == fd_mappings.end())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"DelFd() on unknown fd: %d", fd);
		return false;
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
	return true;
}

int PollEngine::GetMaxFds()
{
#ifndef __FreeBSD__
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
		MAX_DESCRIPTORS = 0;
		ServerInstance->Logs->Log("SOCKET", DEFAULT, "ERROR: Can't determine maximum number of open sockets: %s", strerror(errno));
		printf("ERROR: Can't determine maximum number of open sockets: %s\n", strerror(errno));
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}
	return 0;
#else
	if (!MAX_DESCRIPTORS)
	{
		int mib[2], maxfiles;
		size_t len;

		mib[0] = CTL_KERN;
		mib[1] = KERN_MAXFILES;
		len = sizeof(maxfiles);
		sysctl(mib, 2, &maxfiles, &len, NULL, 0);
		MAX_DESCRIPTORS = maxfiles;
		return maxfiles;
	}
	return MAX_DESCRIPTORS;
#endif
}

int PollEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int PollEngine::DispatchEvents()
{
	int i = poll(events, CurrentSetSize, 1000);
	int index;
	socklen_t codesize = sizeof(int);
	int errcode;
	int processed = 0;

	if (i > 0)
	{
		for (index = 0; index < CurrentSetSize && processed != i; index++)
		{
			if (events[index].revents)
				processed++;

			if (events[index].revents & POLLHUP)
			{
				if (ref[index])
					ref[index]->HandleEvent(EVENT_ERROR, 0);
				continue;
			}

			if (events[index].revents & POLLERR)
			{
				// Get fd
				int fd = events[index].fd;

				// Get error number
				if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &codesize) < 0)
					errcode = errno;
				if (ref[index])
					ref[index]->HandleEvent(EVENT_ERROR, errcode);
				continue;
			}

			if (events[index].revents & POLLOUT)
			{
				// Switch to wanting read again
				// event handlers have to request to write again if they need it
				events[index].events = POLLIN;

				if (ref[index])
					ref[index]->HandleEvent(EVENT_WRITE);
			}

			if (events[index].revents & POLLIN)
			{
				if (ref[index])
					ref[index]->HandleEvent(EVENT_READ);
			}
		}
	}

	return i;
}

std::string PollEngine::GetName()
{
	return "poll";
}

