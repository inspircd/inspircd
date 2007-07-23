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

#include "socketengine_iocp.h"
#include "exitcodes.h"
#include <mswsock.h>

IOCPEngine::IOCPEngine(InspIRCd * Instance) : SocketEngine(Instance)
{
	/* Create completion port */
	m_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, (ULONG_PTR)0, 0);

        if (!m_completionPort)
	{
		ServerInstance->Log(SPARSE,"ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		ServerInstance->Log(SPARSE,"ERROR: this is a fatal error, exiting now.");
		printf("ERROR: Could not initialize socket engine. Your kernel probably does not have the proper features.");
		printf("ERROR: this is a fatal error, exiting now.");
		InspIRCd::Exit(EXIT_STATUS_SOCKETENGINE);
	}

	/* Null variables out. */
	CurrentSetSize = 0;
	EngineHandle = 0;
	memset(ref, 0, sizeof(EventHandler*) * MAX_DESCRIPTORS);
}

IOCPEngine::~IOCPEngine()
{
	CloseHandle(m_completionPort);
}

bool IOCPEngine::AddFd(EventHandler* eh)
{
	/* Does it at least look valid? */
	if (!eh)
		return false;

	int fake_fd = GenerateFd(eh->GetFd());
	int is_accept = 0;
	int opt_len = sizeof(int);

	/* In range? */
	if ((fake_fd < 0) || (fake_fd > MAX_DESCRIPTORS))
		return false;

	/* Already an entry here */
	if (ref[fake_fd])
		return false;

	/* are we a listen socket? */
	getsockopt(eh->GetFd(), SOL_SOCKET, SO_ACCEPTCONN, (char*)&is_accept, &opt_len);

	/* set up the read event so the socket can actually receive data :P */
	eh->m_internalFd = fake_fd;
	eh->m_writeEvent = 0;
	eh->m_acceptEvent = 0;

	unsigned long completion_key = (ULONG_PTR)eh->m_internalFd;
	/* assign the socket to the completion port */
	if (!CreateIoCompletionPort((HANDLE)eh->GetFd(), m_completionPort, completion_key, 0))
		return false;

	/* set up binding, increase set size */
	ref[fake_fd] = eh;
	++CurrentSetSize;

	/* setup initial events */
	if(is_accept)
		PostAcceptEvent(eh);
	else
		PostReadEvent(eh);

	/* log message */
	ServerInstance->Log(DEBUG, "New fake fd: %u, real fd: %u, address 0x%p", fake_fd, eh->GetFd(), eh);

	/* post a write event if there is data to be written */
	if(eh->Writeable())
		WantWrite(eh);

	/* we're all good =) */
	try
	{
		m_binding.insert( map<int, EventHandler*>::value_type( eh->GetFd(), eh ) );
	}
	catch (...)
	{
		/* Ohshi-, map::insert failed :/ */
		return false;
	}

	return true;
}

bool IOCPEngine::DelFd(EventHandler* eh, bool force /* = false */)
{
	if (!eh)
		return false;

	int fake_fd = eh->m_internalFd;
	int fd = eh->GetFd();
	
	if (!ref[fake_fd])
		return false;

	ServerInstance->Log(DEBUG, "Removing fake fd %u, real fd %u, address 0x%p", fake_fd, eh->GetFd(), eh);

	/* Cancel pending i/o operations. */
	if (CancelIo((HANDLE)fd) == FALSE)
		return false;

	/* Free the buffer, and delete the event. */
	if (eh->m_readEvent != 0)
	{
		if(((Overlapped*)eh->m_readEvent)->m_params != 0)
			delete ((udp_overlap*)((Overlapped*)eh->m_readEvent)->m_params);

		delete ((Overlapped*)eh->m_readEvent);
	}

	if(eh->m_writeEvent != 0)
		delete ((Overlapped*)eh->m_writeEvent);

	if(eh->m_acceptEvent != 0)
	{
		delete ((accept_overlap*)((Overlapped*)eh->m_acceptEvent)->m_params);
		delete ((Overlapped*)eh->m_acceptEvent);
	}

	/* Clear binding */
	ref[fake_fd] = 0;
	m_binding.erase(eh->GetFd());

	/* decrement set size */
	--CurrentSetSize;
	
	/* success */
	return true;
}

void IOCPEngine::WantWrite(EventHandler* eh)
{
	if (!eh)
		return;

	/* Post event - write begin */
	if(!eh->m_writeEvent)
	{
		ULONG_PTR completion_key = (ULONG_PTR)eh->m_internalFd;
		Overlapped * ov = new Overlapped(SOCKET_IO_EVENT_WRITE_READY, 0);
		eh->m_writeEvent = (void*)ov;
		PostQueuedCompletionStatus(m_completionPort, 0, completion_key, &ov->m_overlap);
	}
}

bool IOCPEngine::PostCompletionEvent(EventHandler * eh, SocketIOEvent type, int param)
{
	if (!eh)
		return false;

	Overlapped * ov = new Overlapped(type, param);
	ULONG_PTR completion_key = (ULONG_PTR)eh->m_internalFd;
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
	eh->m_readEvent = (void*)ov;
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
		if (intfd < 0 || intfd > MAX_DESCRIPTORS)
			continue;

		// woot, we got an event on a socket :P
		eh = ref[intfd];
		ov = CONTAINING_RECORD(overlap, Overlapped, m_overlap);

		if (eh == 0)
			continue;

		switch(ov->m_event)
		{
			case SOCKET_IO_EVENT_WRITE_READY:
			{
				eh->m_writeEvent = 0;
				eh->HandleEvent(EVENT_WRITE, 0);
			}
			break;

			case SOCKET_IO_EVENT_READ_READY:
			{
				if(ov->m_params)
				{
					// if we had params, it means we are a udp socket with a udp_overlap pointer in this long.
					udp_overlap * uv = (udp_overlap*)ov->m_params;
					uv->udp_len = len;
					this->udp_ov = uv;
					eh->m_readEvent = 0;
					eh->HandleEvent(EVENT_READ, 0);
					this->udp_ov = 0;
					delete uv;
					PostReadEvent(eh);
				}
				else
				{
					ret = ioctlsocket(eh->GetFd(), FIONREAD, &bytes_recv);
					eh->m_readEvent = 0;
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
				eh->HandleEvent(EVENT_READ, ov->m_params);
				delete ((accept_overlap*)ov->m_params);
				eh->m_acceptEvent = 0;
				PostAcceptEvent(eh);
			}
			break;

			case SOCKET_IO_EVENT_ERROR:
			{
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
	eh->m_acceptEvent = (void*)ov;

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

int __accept_socket(SOCKET s, sockaddr * addr, int * addrlen, void * acceptevent)
{
	Overlapped* ovl = (Overlapped*)acceptevent;
	accept_overlap* ov = (accept_overlap*)ovl->m_params;

	sockaddr_in* server_address = (sockaddr_in*)&ov->buf[10];
	sockaddr_in* client_address = (sockaddr_in*)&ov->buf[38];

	memcpy(addr, client_address, sizeof(sockaddr_in));
	*addrlen = sizeof(sockaddr_in);

	return ov->socket;
}

int __getsockname(SOCKET s, sockaddr * name, int * namelen, void * acceptevent)
{
	Overlapped* ovl = (Overlapped*)acceptevent;
	accept_overlap* ov = (accept_overlap*)ovl->m_params;

	sockaddr_in* server_address = (sockaddr_in*)&ov->buf[10];
	sockaddr_in* client_address = (sockaddr_in*)&ov->buf[38];

	memcpy(name, server_address, sizeof(sockaddr_in));
	*namelen = sizeof(sockaddr_in);

	return 0;
}

int __recvfrom(SOCKET s, char * buf, int len, int flags, struct sockaddr * from, int * fromlen, udp_overlap * ov)
{
	memcpy(buf, ov->udp_buffer, ov->udp_len);
	memcpy(from, ov->udp_sockaddr, *fromlen);
	return ov->udp_len;
}

EventHandler * IOCPEngine::GetRef(int fd)
{
	map<int, EventHandler*>::iterator itr = m_binding.find(fd);
	return (itr == m_binding.end()) ? 0 : itr->second;
}

bool IOCPEngine::HasFd(int fd)
{
	return (GetRef(fd) != 0);
}

EventHandler * IOCPEngine::GetIntRef(int fd)
{
	if(fd < 0 || fd > MAX_DESCRIPTORS)
		return 0;
	return ref[fd];
}

