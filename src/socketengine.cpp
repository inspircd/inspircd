/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDsocketengine */

/*********        DEFAULTS       **********/
/* $ExtraSources: socketengines/socketengine_select.cpp */
/* $ExtraObjects: socketengine_select.o */

/* $If: USE_KQUEUE */
/* $ExtraSources: socketengines/socketengine_kqueue.cpp */
/* $ExtraObjects: socketengine_kqueue.o */
/* $EndIf */

/* $If: USE_EPOLL */
/* $ExtraSources: socketengines/socketengine_epoll.cpp */
/* $ExtraObjects: socketengine_epoll.o */
/* $EndIf */

/* $If: USE_PORTS */
/* $ExtraSources: socketengines/socketengine_ports.cpp */
/* $ExtraObjects: socketengine_ports.o */
/* $EndIf */

#include "inspircd.h"
#include "socketengine.h"

int EventHandler::GetFd()
{
	return this->fd;
}

void EventHandler::SetFd(int FD)
{
	this->fd = FD;
}

bool EventHandler::Readable()
{
	return true;
}

bool EventHandler::Writeable()
{
	return false;
}

void SocketEngine::WantWrite(EventHandler* eh)
{
}

SocketEngine::SocketEngine(InspIRCd* Instance) : ServerInstance(Instance)
{
	memset(ref, 0, sizeof(ref));
}

SocketEngine::~SocketEngine()
{
}

bool SocketEngine::AddFd(EventHandler* eh)
{
	return true;
}

bool SocketEngine::HasFd(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;
	return ref[fd];
}

EventHandler* SocketEngine::GetRef(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return 0;
	return ref[fd];
}

bool SocketEngine::DelFd(EventHandler* eh, bool force)
{
	return true;
}

int SocketEngine::GetMaxFds()
{
	return 0;
}

int SocketEngine::GetRemainingFds()
{
	return 0;
}

int SocketEngine::DispatchEvents()
{
	return 0;
}

std::string SocketEngine::GetName()
{
	return "misconfigured";
}

bool SocketEngine::BoundsCheckFd(EventHandler* eh)
{       
	if (!eh)
		return false;
	if ((eh->GetFd() < 0) || (eh->GetFd() > MAX_DESCRIPTORS))
		return false;
	return true;
}

#ifdef WINDOWS

int SocketEngine::Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen) { return -1; }
int SocketEngine::Close(int fd) { return -1; }
int SocketEngine::Close(EventHandler* fd) { return -1; }
int SocketEngine::Blocking(int fd) { return -1; }
int SocketEngine::NonBlocking(int fd) { return -1; }
int SocketEngine::GetSockName(EventHandler* fd, sockaddr *name, socklen_t* namelen) { return -1; }
int SocketEngine::RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen) { return -1; }

#else

int SocketEngine::Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen)
{
	return accept(fd->GetFd(), addr, addrlen);
}

int SocketEngine::Close(EventHandler* fd)
{
	return close(fd->GetFd());
}

int SocketEngine::Close(int fd)
{
	return close(fd);
}

int SocketEngine::Blocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags ^ O_NONBLOCK);
}

int SocketEngine::NonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int SocketEngine::GetSockName(EventHandler* fd, sockaddr *name, socklen_t* namelen)
{
	return getsockname(fd->GetFd(), name, namelen);
}

int SocketEngine::RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen)
{
	return recvfrom(fd->GetFd(), buf, len, flags, from, fromlen);
}

#endif

int SocketEngine::Send(EventHandler* fd, const void *buf, size_t len, int flags)
{
	return send(fd->GetFd(), (const char*)buf, len, flags);
}

int SocketEngine::Recv(EventHandler* fd, void *buf, size_t len, int flags)
{
	return recv(fd->GetFd(), (char*)buf, len, flags);
}

int SocketEngine::SendTo(EventHandler* fd, const void *buf, size_t len, int flags, const sockaddr *to, socklen_t tolen)
{
	return sendto(fd->GetFd(), (const char*)buf, len, flags, to, tolen);
}

int SocketEngine::Connect(EventHandler* fd, const sockaddr *serv_addr, socklen_t addrlen)
{
	return connect(fd->GetFd(), serv_addr, addrlen);
}

int SocketEngine::Shutdown(EventHandler* fd, int how)
{
	return shutdown(fd->GetFd(), how);
}

int SocketEngine::Bind(int fd, const sockaddr *my_addr, socklen_t addrlen)
{
	return bind(fd, my_addr, addrlen);
}

int SocketEngine::Listen(int sockfd, int backlog)
{
	return listen(sockfd, backlog);
}

int SocketEngine::Shutdown(int fd, int how)
{
	return shutdown(fd, how);
}

void SocketEngine::RecoverFromFork()
{
}

