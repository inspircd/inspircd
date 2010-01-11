/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __SOCKETENGINE_PORTS__
#define __SOCKETENGINE_PORTS__

#ifndef __sun
# error You need Solaris 10 or later to make use of this code.
#endif

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include "socketengine.h"
#include <port.h>

/** A specialisation of the SocketEngine class, designed to use solaris 10 I/O completion ports
 */
class PortsEngine : public SocketEngine
{
private:
	/** These are used by epoll() to hold socket events
	 */
	port_event_t* events;
public:
	/** Create a new PortsEngine
	 * @param Instance The creator of this object
	 */
	PortsEngine();
	/** Delete a PortsEngine
	 */
	virtual ~PortsEngine();
	virtual bool AddFd(EventHandler* eh, int event_mask);
	void OnSetEvent(EventHandler* eh, int old_event, int new_event);
	virtual bool DelFd(EventHandler* eh, bool force = false);
	virtual int DispatchEvents();
	virtual std::string GetName();
	virtual void WantWrite(EventHandler* eh);
};

#endif


#include <ulimit.h>

PortsEngine::PortsEngine()
{
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

static int mask_to_events(int event_mask)
{
	int rv = 0;
	if (event_mask & (FD_WANT_POLL_READ | FD_WANT_FAST_READ))
		rv |= POLLRDNORM;
	if (event_mask & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
		rv |= POLLWRNORM;
	return rv;
}

bool PortsEngine::AddFd(EventHandler* eh, int event_mask)
{
	int fd = eh->GetFd();
	if ((fd < 0) || (fd > GetMaxFds() - 1))
		return false;

	if (ref[fd])
		return false;

	ref[fd] = eh;
	SocketEngine::SetEventMask(eh, event_mask);
	port_associate(EngineHandle, PORT_SOURCE_FD, fd, mask_to_events(event_mask), eh);

	ServerInstance->Logs->Log("SOCKET",DEBUG,"New file descriptor: %d", fd);
	CurrentSetSize++;
	return true;
}

void PortsEngine::WantWrite(EventHandler* eh, int old_mask, int new_mask)
{
	if (mask_to_events(new_mask) != mask_to_events(old_mask))
		port_associate(EngineHandle, PORT_SOURCE_FD, eh->GetFd(), mask_to_events(new_mask), eh);
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
				EventHandler* eh = ref[fd];
				if (eh)
				{
					int mask = eh->GetEventMask();
					if (events[i].portev_events & POLLWRNORM)
						mask &= ~(FD_WRITE_WILL_BLOCK | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE);
					if (events[i].portev_events & POLLRDNORM)
						mask &= ~FD_READ_WILL_BLOCK;
					// reinsert port for next time around, pretending to be one-shot for writes
					SetEventMask(ev, mask);
					port_associate(EngineHandle, PORT_SOURCE_FD, fd, mask_to_events(mask), eh);
					if (events[i].portev_events & POLLRDNORM)
					{
						ReadEvents++;
						eh->HandleEvent(EVENT_READ);
					}
					if (events[i].portev_events & POLLWRNORM)
					{
						WriteEvents++;
						eh->HandleEvent(EVENT_WRITE);
					}
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

SocketEngine* CreateSocketEngine()
{
	return new PortsEngine;
}
