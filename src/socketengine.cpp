/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Burlex <???@???>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


/* $Core */

/*********        DEFAULTS       **********/

/* $ExtraSources: socketengines/socketengine_select.cpp */
/* $ExtraObjects: socketengine_select.o */

/* $If: USE_POLL */
/* $ExtraSources: socketengines/socketengine_poll.cpp */
/* $ExtraObjects: socketengine_poll.o */
/* $EndIf */

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

EventHandler::EventHandler()
{
	this->IOHook = NULL;
}

bool EventHandler::AddIOHook(Module *IOHooker)
{
	if (this->IOHook)
		return false;

	this->IOHook = IOHooker;
	return true;
}

bool EventHandler::DelIOHook()
{
	if (!this->IOHook)
		return false;

	this->IOHook = NULL;
	return true;
}

Module *EventHandler::GetIOHook()
{
	return this->IOHook;
}

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
	TotalEvents = WriteEvents = ReadEvents = ErrorEvents = 0;
	lastempty = ServerInstance->Time();
	indata = outdata = 0;
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


int SocketEngine::Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen)
{
	return accept(fd->GetFd(), addr, addrlen);
}

int SocketEngine::Close(EventHandler* fd)
{
#ifdef WINDOWS
	return closesocket(fd->GetFd());
#else
	return close(fd->GetFd());
#endif
}

int SocketEngine::Close(int fd)
{
#ifdef WINDOWS
	return closesocket(fd);
#else
	return close(fd);
#endif
}

int SocketEngine::Blocking(int fd)
{
#ifdef WINDOWS
	unsigned long opt = 0;
	return ioctlsocket(fd, FIONBIO, &opt);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags ^ O_NONBLOCK);
#endif
}

int SocketEngine::NonBlocking(int fd)
{
#ifdef WINDOWS
	unsigned long opt = 1;
	return ioctlsocket(fd, FIONBIO, &opt);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

int SocketEngine::GetSockName(EventHandler* fd, sockaddr *name, socklen_t* namelen)
{
	return getsockname(fd->GetFd(), name, namelen);
}

int SocketEngine::RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen)
{
	this->UpdateStats(len, 0);
	return recvfrom(fd->GetFd(), (char*)buf, len, flags, from, fromlen);
}

int SocketEngine::Send(EventHandler* fd, const void *buf, size_t len, int flags)
{
	this->UpdateStats(0, len);
	return send(fd->GetFd(), (const char*)buf, len, flags);
}

int SocketEngine::Recv(EventHandler* fd, void *buf, size_t len, int flags)
{
	this->UpdateStats(len, 0);
	return recv(fd->GetFd(), (char*)buf, len, flags);
}

int SocketEngine::SendTo(EventHandler* fd, const void *buf, size_t len, int flags, const sockaddr *to, socklen_t tolen)
{
	this->UpdateStats(0, len);
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

void SocketEngine::UpdateStats(size_t len_in, size_t len_out)
{
	if (lastempty != ServerInstance->Time())
	{
		lastempty = ServerInstance->Time();
		indata = outdata = 0;
	}
	indata += len_in;
	outdata += len_out;
}

void SocketEngine::GetStats(float &kbitpersec_in, float &kbitpersec_out, float &kbitpersec_total)
{
	UpdateStats(0, 0); /* Forces emptying of the values if its been more than a second */
	float in_kbit = indata * 8;
	float out_kbit = outdata * 8;
	kbitpersec_total = ((in_kbit + out_kbit) / 1024);
	kbitpersec_in = in_kbit / 1024;
	kbitpersec_out = out_kbit / 1024;
}
