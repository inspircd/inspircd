/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include <port.h>
#include "socketengine_ports.h"

PortsEngine::PortsEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	EngineHandle = port_create();

	if (EngineHandle == -1)
	{
		ServerInstance->Log(SPARSE,"ERROR: Could not initialize socket engine: %s", strerror(errno));
		ServerInstance->Log(SPARSE,"ERROR: This is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine: %s\n", strerror(errno));
		printf("ERROR: This is a fatal error, exiting now.\n");
		InspIRCd::Exit(EXIT_STATUS_SOCKETENGINE);
	}
	CurrentSetSize = 0;
}

PortsEngine::~PortsEngine()
{
	close(EngineHandle);
}

bool PortsEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	port_associate(EngineHandle, PORT_SOURCE_FD, fd, eh->Readable() ? POLLRDNORM : POLLWRNORM, eh);

	ServerInstance->Log(DEBUG,"New file descriptor: %d", fd);
	CurrentSetSize++;
	return true;
}

void PortsEngine::WantWrite(EventHandler* eh)
{
	port_associate(EngineHandle, PORT_SOURCE_FD, eh->GetFd(), POLLWRNORM, eh);
}

bool PortsEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	port_dissociate(EngineHandle, PORT_SOURCE_FD, fd);

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Log(DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

int PortsEngine::GetMaxFds()
{
	return MAX_DESCRIPTORS;
}

int PortsEngine::GetRemainingFds()
{
	return MAX_DESCRIPTORS - CurrentSetSize;
}

int PortsEngine::DispatchEvents()
{
	struct timespec poll_time;

	poll_time.tv_sec = 1;
	poll_time.tv_nsec = 0;

	unsigned int nget = 1; // used to denote a retrieve request.
	int i = port_getn(EngineHandle, this->events, MAX_DESCRIPTORS, &nget, &poll_time);

	// first handle an error condition
	if (i == -1)
		return i;

	for (i = 0; i < nget; i++)
	{
		switch (this->events[i].portev_source)
		{
			case PORT_SOURCE_FD:
			{
				int fd = this->events[i].portev_object;
				if (ref[fd])
				{
					// reinsert port for next time around
					port_associate(EngineHandle, PORT_SOURCE_FD, fd, POLLRDNORM, ref[fd]);
					ref[fd]->HandleEvent((this->events[i].portev_events & POLLRDNORM) ? EVENT_READ : EVENT_WRITE);
				}
			}
			default:
			break;
		}
	}

	return i;
}

std::string PortsEngine::GetName()
{
	return "ports";
}

