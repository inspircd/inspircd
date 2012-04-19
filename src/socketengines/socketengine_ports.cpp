/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "exitcodes.h"
#include <port.h>
#include "socketengines/socketengine_ports.h"
#include <ulimit.h>

PortsEngine::PortsEngine(InspIRCd* Instance) : SocketEngine(Instance)
{
	MAX_DESCRIPTORS = 0;
	EngineHandle = port_create();

	if (EngineHandle == -1)
	{
		ServerInstance->Logs->Log("SOCKET",SPARSE,"ERROR: Could not initialize socket engine: %s", strerror(errno));
		ServerInstance->Logs->Log("SOCKET",SPARSE,"ERROR: This is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine: %s\n", strerror(errno));
		printf("ERROR: This is a fatal error, exiting now.\n");
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}
	CurrentSetSize = 0;

	ref = new EventHandler* [GetMaxFds()];
	events = new port_event_t[GetMaxFds()];
	memset(ref, 0, GetMaxFds() * sizeof(EventHandler*));
}

PortsEngine::~PortsEngine()
{
	this->Close(EngineHandle);
	delete[] ref;
	delete[] events;
}

bool PortsEngine::AddFd(EventHandler* eh)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (GetRemainingFds() <= 1)
		return false;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	port_associate(EngineHandle, PORT_SOURCE_FD, fd, eh->Readable() ? POLLRDNORM : POLLWRNORM, eh);

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);
	CurrentSetSize++;
	return true;
}

void PortsEngine::WantWrite(EventHandler* eh)
{
	port_associate(EngineHandle, PORT_SOURCE_FD, eh->GetFd(), POLLRDNORM | POLLWRNORM, eh);
}

bool PortsEngine::DelFd(EventHandler* eh, bool force)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	port_dissociate(EngineHandle, PORT_SOURCE_FD, fd);

	CurrentSetSize--;
	ref[fd] = NULL;

	ServerInstance->Logs->Log("SOCKET",DEBUG,"Remove file descriptor: %d", fd);
	return true;
}

int PortsEngine::GetMaxFds()
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
#include <ulimit.h>
}

int PortsEngine::GetRemainingFds()
{
	return GetMaxFds() - CurrentSetSize;
}

int PortsEngine::DispatchEvents()
{
	struct timespec poll_time;

	poll_time.tv_sec = 1;
	poll_time.tv_nsec = 0;

	unsigned int nget = 1; // used to denote a retrieve request.
	int i = port_getn(EngineHandle, this->events, GetMaxFds() - 1, &nget, &poll_time);

	// first handle an error condition
	if (i == -1)
		return i;

	TotalEvents += nget;

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
					if ((this->events[i].portev_events & POLLRDNORM))
						ReadEvents++;
					else
						WriteEvents++;
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

