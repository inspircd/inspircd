/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef __SOCKETENGINE_IOCP__
#define __SOCKETENGINE_IOCP__

#define READ_BUFFER_SIZE 600
#define USING_IOCP 1

#include "inspircd_config.h"
#include "inspircd_win32wrapper.h"
#include "inspircd.h"
#include "socketengine.h"

/** Socket overlapped event types
 */
enum SocketIOEvent
{
	/** Read ready */
	SOCKET_IO_EVENT_READ_READY			= 0,
	/** Write ready */
	SOCKET_IO_EVENT_WRITE_READY			= 1,
	/** Accept ready */
	SOCKET_IO_EVENT_ACCEPT				= 2,
	/** Error occured */
	SOCKET_IO_EVENT_ERROR				= 3,
	/** Number of events */
	NUM_SOCKET_IO_EVENTS				= 4,
};

/** Represents a windows overlapped IO event
 */
class Overlapped
{
 public:
	/** Overlap event */
	OVERLAPPED m_overlap;
	/** Type of event */
	SocketIOEvent m_event;
#ifdef WIN64
	/** Parameters */
	unsigned __int64 m_params;
#else
	/** Parameters */
	unsigned long m_params;
#endif
	/** Create an overlapped event
	 */
	Overlapped(SocketIOEvent ev, int params) : m_event(ev), m_params(params)
	{
		memset(&m_overlap, 0, sizeof(OVERLAPPED));
	}
};

/** Specific to UDP sockets with overlapped IO
 */
struct udp_overlap
{
	unsigned char udp_buffer[600];
	unsigned long udp_len;
	sockaddr udp_sockaddr[2];
	unsigned long udp_sockaddr_len;
};

/** Specific to accepting sockets with overlapped IO
 */
struct accept_overlap
{
	int socket;
	char buf[1024];
};

/** Implementation of SocketEngine that implements windows IO Completion Ports
 */
class IOCPEngine : public SocketEngine
{
	/** Creates a "fake" file descriptor for use with an IOCP socket.
	 * This is a little slow, but it isnt called too much. We'll fix it
	 * in a future release.
	 * @return -1 if there are no free slots, and an integer if it finds one.
	 */
	__inline int GenerateFd(int RealFd)
	{
		int index_hash = RealFd % MAX_DESCRIPTORS;
		if(ref[index_hash] == 0)
			return index_hash;
		else
		{
			register int i = 0;
			for(; i < MAX_DESCRIPTORS; ++i)
				if(ref[i] == 0)
					return i;
		}
		return -1;
	}

	/** Global I/O completion port that sockets attach to.
	 */
	HANDLE m_completionPort;

	/** This is kinda shitty... :/ for getting an address from a real fd.
	 */
	std::map<int, EventHandler*> m_binding;

public:
	/** Holds the preallocated buffer passed to WSARecvFrom
	 * function. Yes, I know, it's a dirty hack.
	 */
	udp_overlap * udp_ov;

	/** Creates an IOCP Socket Engine
	 * @param Instance The creator of this object
	 */
	IOCPEngine(InspIRCd* Instance);

	/** Deletes an IOCP socket engine and all the attached sockets
	 */
	~IOCPEngine();

	/** Adds an event handler to the completion port, and sets up initial events.
	 * @param eh EventHandler to add
	 * @return True if success, false if no room
	 */
	bool AddFd(EventHandler* eh);

	/** Gets the maximum number of file descriptors that this engine can handle.
	 * @return The number of file descriptors
	 */
	__inline int GetMaxFds() { return MAX_DESCRIPTORS; }

	/** Gets the number of free/remaining file descriptors under this engine.
	 * @return Remaining count
	 */
	__inline int GetRemainingFds()
	{
		register int count = 0;
		register int i = 0;
		for(; i < MAX_DESCRIPTORS; ++i)
			if(ref[i] == 0)
				++count;
		return count;
	}

	/** Removes a file descriptor from the set, preventing it from receiving any more events
	 * @return True if remove was successful, false otherwise
	 */
	bool DelFd(EventHandler* eh, bool force = false);

	/** Called every loop to handle input/output events for all sockets under this engine
	 * @return The number of "changed" sockets.
	 */
	int DispatchEvents();

	/** Gets the name of this socket engine as a string.
	 * @return string of socket engine name
	 */
	std::string GetName();

	/** Queues a Write event on the specified event handler.
	 * @param eh EventHandler that needs data sent on
	 */
	void WantWrite(EventHandler* eh);

	/** Posts a completion event on the specified socket.
	 * @param eh EventHandler for message
	 * @param type Event Type
	 * @param param Event Parameter
	 * @return True if added, false if not
	 */
	bool PostCompletionEvent(EventHandler* eh, SocketIOEvent type, int param);

	/** Posts a read event on the specified socket
	 * @param eh EventHandler (socket)
	 */
	void PostReadEvent(EventHandler* eh);

	/** Posts an accept event on the specified socket
	 * @param eh EventHandler (socket)
	 */
	void PostAcceptEvent(EventHandler* eh);

	/** Returns the EventHandler attached to a specific fd.
	 * If the fd isnt in the socketengine, returns NULL.
	 * @param fd The event handler to look for
	 * @return A pointer to the event handler, or NULL
	 */
	EventHandler* GetRef(int fd);

	/** Returns true if a file descriptor exists in
	 * the socket engine's list.
	 * @param fd The event handler to look for
	 * @return True if this fd has an event handler
	 */
	bool HasFd(int fd);

	/** Returns the EventHandler attached to a specific fd.
	 * If the fd isnt in the socketengine, returns NULL.
	 * @param fd The event handler to look for
	 * @return A pointer to the event handler, or NULL
	 */
	EventHandler* GetIntRef(int fd);

	bool BoundsCheckFd(EventHandler* eh);

	virtual int Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen);

	virtual int RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);

	virtual int Blocking(int fd);

	virtual int NonBlocking(int fd);

	virtual int GetSockName(EventHandler* fd, sockaddr *name, socklen_t* namelen);

	virtual int Close(int fd);

	virtual int Close(EventHandler* fd);
};

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
public:
	/** Create a new instance of SocketEngine based on IOCPEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new IOCPEngine(Instance); }
};

#endif
