/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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

	LocalIntExt fdExt;
	LocalIntExt readExt;
	LocalIntExt writeExt;
	LocalIntExt acceptExt;

public:
	/** Holds the preallocated buffer passed to WSARecvFrom
	 * function. Yes, I know, it's a dirty hack.
	 */
	udp_overlap * udp_ov;

	/** Creates an IOCP Socket Engine
	 * @param Instance The creator of this object
	 */
	IOCPEngine();

	/** Deletes an IOCP socket engine and all the attached sockets
	 */
	~IOCPEngine();

	/** Adds an event handler to the completion port, and sets up initial events.
	 * @param eh EventHandler to add
	 * @return True if success, false if no room
	 */
	bool AddFd(EventHandler* eh, int event_mask);

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

	void OnSetEvent(EventHandler* eh, int old_mask, int new_mask);

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

#endif

#include "exitcodes.h"
#include <mswsock.h>

IOCPEngine::IOCPEngine()
: fdExt("internal_fd", NULL),
  readExt("windows_readevent", NULL),
  writeExt("windows_writeevent", NULL),
  acceptExt("windows_acceptevent", NULL)
{
	MAX_DESCRIPTORS = 10240;

	/* Create completion port */
	m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);

	if (!m_completionPort)
	{
		ServerInstance->Logs->Log("SOCKET",DEFAULT, "ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		ServerInstance->Logs->Log("SOCKET",DEFAULT, "ERROR: this is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.\n");
		printf("ERROR: this is a fatal error, exiting now.\n");
		ServerInstance->Exit(EXIT_STATUS_SOCKETENGINE);
	}

	/* Null variables out. */
	CurrentSetSize = 0;
	MAX_DESCRIPTORS = 10240;
	ref = new EventHandler* [10240];
	memset(ref, 0, sizeof(EventHandler*) * MAX_DESCRIPTORS);
}

IOCPEngine::~IOCPEngine()
{
	/* Clean up winsock and close completion port */
	CloseHandle(m_completionPort);
	WSACleanup();
	delete[] ref;
}

bool IOCPEngine::AddFd(EventHandler* eh, int event_mask)
{
	/* Does it at least look valid? */
	if (!eh)
		return false;

	int* fake_fd = new int(GenerateFd(eh->GetFd()));
	int is_accept = 0;
	int opt_len = sizeof(int);

	/* In range? */
	if ((*fake_fd < 0) || (*fake_fd > MAX_DESCRIPTORS))
	{
		delete fake_fd;
		return false;
	}

	/* Already an entry here */
	if (ref[*fake_fd])
	{
		delete fake_fd;
		return false;
	}

	/* are we a listen socket? */
	getsockopt(eh->GetFd(), SOL_SOCKET, SO_ACCEPTCONN, (char*)&is_accept, &opt_len);

	/* set up the read event so the socket can actually receive data :P */
	fdExt.set(eh, *fake_fd);

	unsigned long completion_key = (ULONG_PTR)*fake_fd;
	/* assign the socket to the completion port */
	if (!CreateIoCompletionPort((HANDLE)eh->GetFd(), m_completionPort, completion_key, 0))
		return false;

	/* setup initial events */
	if(is_accept)
		PostAcceptEvent(eh);
	else
		PostReadEvent(eh);

	/* log message */
	ServerInstance->Logs->Log("SOCKET",DEBUG, "New fake fd: %u, real fd: %u, address 0x%p", *fake_fd, eh->GetFd(), eh);

	/* post a write event if there is data to be written */
	if (event_mask & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE))
		OnSetEvent(eh, event_mask, event_mask);

	/* we're all good =) */
	try
	{
		m_binding.insert( std::map<int, EventHandler*>::value_type( eh->GetFd(), eh ) );
	}
	catch (...)
	{
		/* Ohshi-, map::insert failed :/ */
		return false;
	}

	++CurrentSetSize;
	SocketEngine::SetEventMask(eh, event_mask);
	ref[*fake_fd] = eh;

	return true;
}

bool IOCPEngine::DelFd(EventHandler* eh, bool force /* = false */)
{
	if (!eh)
		return false;

	int* fake_fd = (int*)fdExt.get(eh);

	if (!fake_fd)
		return false;

	int fd = eh->GetFd();

	void* m_readEvent = (void*)readExt.get(eh);
	void* m_writeEvent = (void*)writeExt.get(eh);
	void* m_acceptEvent = (void*)acceptExt.get(eh);

	ServerInstance->Logs->Log("SOCKET",DEBUG, "Removing fake fd %u, real fd %u, address 0x%p", *fake_fd, eh->GetFd(), eh);

	/* Cancel pending i/o operations. */
	if (CancelIo((HANDLE)fd) == FALSE)
		return false;

	/* Free the buffer, and delete the event. */
	if (m_readEvent)
	{
		if(((Overlapped*)m_readEvent)->m_params != 0)
			delete ((udp_overlap*)((Overlapped*)m_readEvent)->m_params);

		delete ((Overlapped*)m_readEvent);
		readExt.free(eh);
	}

	if(m_writeEvent)
	{
		delete ((Overlapped*)m_writeEvent);
		writeExt.free(eh);
	}

	if(m_acceptEvent)
	{
		delete ((accept_overlap*)((Overlapped*)m_acceptEvent)->m_params);
		delete ((Overlapped*)m_acceptEvent);
		acceptExt.free(eh);
	}

	/* Clear binding */
	ref[*fake_fd] = 0;
	m_binding.erase(eh->GetFd());

	delete fake_fd;
	fdExt.free(eh);

	/* decrement set size */
	--CurrentSetSize;

	/* success */
	return true;
}

void IOCPEngine::OnSetEvent(EventHandler* eh, int old_mask, int new_mask)
{
	if (!eh)
		return;

	void* m_writeEvent = (void*)writeExt.get(eh);

	int* fake_fd = (int*)fdExt.get(eh);
	if (!fake_fd)
		return;

	/* Post event - write begin */
	if((new_mask & (FD_WANT_POLL_WRITE | FD_WANT_FAST_WRITE | FD_WANT_SINGLE_WRITE)) && !m_writeEvent)
	{
		ULONG_PTR completion_key = (ULONG_PTR)*fake_fd;
		Overlapped * ov = new Overlapped(SOCKET_IO_EVENT_WRITE_READY, 0);
		writeExt.free(eh);
		writeExt.set(eh, (intptr_t)ov);
		PostQueuedCompletionStatus(m_completionPort, 0, completion_key, &ov->m_overlap);
	}
}

bool IOCPEngine::PostCompletionEvent(EventHandler * eh, SocketIOEvent type, int param)
{
	if (!eh)
		return false;

	int* fake_fd = (int*)fdExt.get(eh);
	if (!fake_fd)
		return false;

	Overlapped * ov = new Overlapped(type, param);
	ULONG_PTR completion_key = (ULONG_PTR)*fake_fd;
	return PostQueuedCompletionStatus(m_completionPort, 0, completion_key, &ov->m_overlap);
}

void IOCPEngine::PostReadEvent(EventHandler * eh)
{
	if (!eh)
		return;

	Overlapped * ov = new Overlapped(SOCKET_IO_EVENT_READ_READY, 0);
	DWORD flags = 0;
	DWORD r_length = 0;
	WSABUF buf;

	/* by passing a null buffer pointer, we can have this working in the same way as epoll..
	 * its slower, but it saves modifying all network code.
	 */
	buf.buf = 0;
	buf.len = 0;

	/* determine socket type. */
	DWORD sock_type;
	int sock_len = sizeof(DWORD);
	if(getsockopt(eh->GetFd(), SOL_SOCKET, SO_TYPE, (char*)&sock_type, &sock_len) == -1)
	{
		/* wtfhax? */
		PostCompletionEvent(eh, SOCKET_IO_EVENT_ERROR, 0);
		delete ov;
		return;
	}
	switch(sock_type)
	{
		case SOCK_DGRAM:			/* UDP Socket */
		{
			udp_overlap * uv = new udp_overlap;
			uv->udp_sockaddr_len = sizeof(sockaddr);
			buf.buf = (char*)uv->udp_buffer;
			buf.len = sizeof(uv->udp_buffer);
			ov->m_params = (unsigned long)uv;
			if(WSARecvFrom(eh->GetFd(), &buf, 1, &uv->udp_len, &flags, uv->udp_sockaddr, (LPINT)&uv->udp_sockaddr_len, &ov->m_overlap, 0))
			{
				int err = WSAGetLastError();
				if(err != WSA_IO_PENDING)
				{
					delete ov;
					PostCompletionEvent(eh, SOCKET_IO_EVENT_ERROR, 0);
					return;
				}
			}
		}
		break;

		case SOCK_STREAM:			/* TCP Socket */
		{
			if(WSARecv(eh->GetFd(), &buf, 1, &r_length, &flags, &ov->m_overlap, 0) == SOCKET_ERROR)
			{
				if(WSAGetLastError() != WSA_IO_PENDING)
				{
					delete ov;
					PostCompletionEvent(eh, SOCKET_IO_EVENT_ERROR, 0);
					return;
				}
			}
		}
		break;

		default:
		{
			printf("unknwon socket type: %u\n", sock_type);
			return;
		}
		break;
	}
	readExt.set(eh, (intptr_t)ov);
}

int IOCPEngine::DispatchEvents()
{
	DWORD len;
	LPOVERLAPPED overlap;
	Overlapped * ov;
	EventHandler * eh;
	ULONG_PTR intfd;
	int ret;
	unsigned long bytes_recv;

	while (GetQueuedCompletionStatus(m_completionPort, &len, &intfd, &overlap, 1000))
	{
		if (intfd > (unsigned long)MAX_DESCRIPTORS)
			continue;

		// woot, we got an event on a socket :P
		eh = ref[intfd];
		ov = CONTAINING_RECORD(overlap, Overlapped, m_overlap);

		if (eh == 0)
			continue;

		TotalEvents++;

		switch(ov->m_event)
		{
			case SOCKET_IO_EVENT_WRITE_READY:
			{
				WriteEvents++;
				writeExt.free(eh);
				SetEventMask(eh, eh->GetEventMask() & ~FD_WRITE_WILL_BLOCK);
				eh->HandleEvent(EVENT_WRITE, 0);
			}
			break;

			case SOCKET_IO_EVENT_READ_READY:
			{
				ReadEvents++;
				SetEventMask(eh, eh->GetEventMask() & ~FD_READ_WILL_BLOCK);
				if(ov->m_params)
				{
					// if we had params, it means we are a udp socket with a udp_overlap pointer in this long.
					udp_overlap * uv = (udp_overlap*)ov->m_params;
					uv->udp_len = len;
					this->udp_ov = uv;
					readExt.free(eh);
					eh->HandleEvent(EVENT_READ, 0);
					this->udp_ov = 0;
					delete uv;
					PostReadEvent(eh);
				}
				else
				{
					ret = ioctlsocket(eh->GetFd(), FIONREAD, &bytes_recv);
					readExt.free(eh);
					if(ret != 0 || bytes_recv == 0)
					{
						/* end of file */
						PostCompletionEvent(eh, SOCKET_IO_EVENT_ERROR, EIO); /* Old macdonald had an error, EIEIO. */
					}
					else
					{
						eh->HandleEvent(EVENT_READ, 0);
						PostReadEvent(eh);
					}
				}
			}
			break;

			case SOCKET_IO_EVENT_ACCEPT:
			{
				/* this is kinda messy.. :/ */
				ReadEvents++;
				eh->HandleEvent(EVENT_READ, ov->m_params);
				delete ((accept_overlap*)ov->m_params);
				acceptExt.free(eh);
				PostAcceptEvent(eh);
			}
			break;

			case SOCKET_IO_EVENT_ERROR:
			{
				ErrorEvents++;
				eh->HandleEvent(EVENT_ERROR, ov->m_params);
			}
			break;
		}

		delete ov;
	}

	return 0;
}

void IOCPEngine::PostAcceptEvent(EventHandler * eh)
{
	if (!eh)
		return;

	int on = 1;
	u_long arg = 1;
	struct linger linger = { 0 };

	int fd = WSASocket(AF_INET, SOCK_STREAM, 0, 0, 0, WSA_FLAG_OVERLAPPED);

	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
	/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
	linger.l_onoff = 1;
	linger.l_linger = 1;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&linger,sizeof(linger));
	ioctlsocket(fd, FIONBIO, &arg);

	int len = sizeof(sockaddr_in) + 16;
	DWORD dwBytes;
	accept_overlap* ao = new accept_overlap;
	memset(ao->buf, 0, 1024);
	ao->socket = fd;

	Overlapped* ov = new Overlapped(SOCKET_IO_EVENT_ACCEPT, (int)ao);
	acceptExt.set(eh, (intptr_t)ov);

	if(AcceptEx(eh->GetFd(), fd, ao->buf, 0, len, len, &dwBytes, &ov->m_overlap) == FALSE)
	{
		int err = WSAGetLastError();
		if(err != WSA_IO_PENDING)
		{
			printf("PostAcceptEvent err: %d\n", err);
		}
	}
}


std::string IOCPEngine::GetName()
{
	return "iocp";
}

EventHandler * IOCPEngine::GetRef(int fd)
{
	std::map<int, EventHandler*>::iterator itr = m_binding.find(fd);
	return (itr == m_binding.end()) ? 0 : itr->second;
}

bool IOCPEngine::HasFd(int fd)
{
	return (GetRef(fd) != 0);
}

bool IOCPEngine::BoundsCheckFd(EventHandler* eh)
{
	int* internal_fd = (int*)fdExt.get(eh);
	if (!eh || eh->GetFd() < 0)
		return false;

	if(!internal_fd)
		return false;

	if(*internal_fd > MAX_DESCRIPTORS)
		return false;

	return true;
}

EventHandler * IOCPEngine::GetIntRef(int fd)
{
	if(fd < 0 || fd > MAX_DESCRIPTORS)
		return 0;
	return ref[fd];
}

int IOCPEngine::Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen)
{
	//SOCKET s = fd->GetFd();

	Overlapped* acceptevent = (Overlapped*)acceptExt.get(fd);
	if (!acceptevent)
		/* Shit, no accept event on this socket! :( */
		return -1;

	Overlapped* ovl = acceptevent;
	accept_overlap* ov = (accept_overlap*)ovl->m_params;

	//sockaddr_in* server_address = (sockaddr_in*)&ov->buf[10];
	sockaddr_in* client_address = (sockaddr_in*)&ov->buf[38];

	memcpy(addr, client_address, sizeof(sockaddr_in));
	*addrlen = sizeof(sockaddr_in);

	return ov->socket;
}

int IOCPEngine::GetSockName(EventHandler* fd, sockaddr *name, socklen_t* namelen)
{
	Overlapped* ovl = (Overlapped*)acceptExt.get(fd);

	if (!ovl)
		return -1;

	accept_overlap* ov = (accept_overlap*)ovl->m_params;

	sockaddr_in* server_address = (sockaddr_in*)&ov->buf[10];
	//sockaddr_in* client_address = (sockaddr_in*)&ov->buf[38];

	memcpy(name, server_address, sizeof(sockaddr_in));
	*namelen = sizeof(sockaddr_in);

	return 0;
}

int IOCPEngine::RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	this->UpdateStats(len, 0);
	udp_overlap* ov = (udp_overlap*)readExt.get(fd);
	if (!ov)
		return -1;
        memcpy(buf, ov->udp_buffer, ov->udp_len);
	memcpy(from, ov->udp_sockaddr, *fromlen);
	return ov->udp_len;
}

int IOCPEngine::Blocking(int fd)
{
	unsigned long opt = 0;
	return ioctlsocket(fd, FIONBIO, &opt);
}

int IOCPEngine::NonBlocking(int fd)
{
	unsigned long opt = 1;
	return ioctlsocket(fd, FIONBIO, &opt);
}

int IOCPEngine::Close(int fd)
{
	return closesocket(fd);
}

int IOCPEngine::Close(EventHandler* fd)
{
	return this->Close(fd->GetFd());
}

SocketEngine* CreateSocketEngine()
{
	return new IOCPEngine;
}
