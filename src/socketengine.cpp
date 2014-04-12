/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#include "inspircd.h"

EventHandler::EventHandler()
{
	fd = -1;
	event_mask = 0;
}

void EventHandler::SetFd(int FD)
{
	this->fd = FD;
}

SocketEngine::SocketEngine()
{
	TotalEvents = WriteEvents = ReadEvents = ErrorEvents = 0;
	lastempty = ServerInstance->Time();
	indata = outdata = 0;
}

SocketEngine::~SocketEngine()
{
}

void SocketEngine::SetEventMask(EventHandler* eh, int mask)
{
	eh->event_mask = mask;
}

void SocketEngine::ChangeEventMask(EventHandler* eh, int change)
{
	int old_m = eh->event_mask;
	int new_m = old_m;

	// if we are changing read/write type, remove the previously set bit
	if (change & FD_WANT_READ_MASK)
		new_m &= ~FD_WANT_READ_MASK;
	if (change & FD_WANT_WRITE_MASK)
		new_m &= ~FD_WANT_WRITE_MASK;
	
	// if adding a trial read/write, insert it into the set
	if (change & FD_TRIAL_NOTE_MASK && !(old_m & FD_TRIAL_NOTE_MASK))
		trials.insert(eh->GetFd());

	new_m |= change;
	if (new_m == old_m)
		return;

	eh->event_mask = new_m;
	OnSetEvent(eh, old_m, new_m);
}

void SocketEngine::DispatchTrialWrites()
{
	std::vector<int> working_list;
	working_list.reserve(trials.size());
	working_list.assign(trials.begin(), trials.end());
	trials.clear();
	for(unsigned int i=0; i < working_list.size(); i++)
	{
		int fd = working_list[i];
		EventHandler* eh = GetRef(fd);
		if (!eh)
			continue;
		int mask = eh->event_mask;
		eh->event_mask &= ~(FD_ADD_TRIAL_READ | FD_ADD_TRIAL_WRITE);
		if ((mask & (FD_ADD_TRIAL_READ | FD_READ_WILL_BLOCK)) == FD_ADD_TRIAL_READ)
			eh->HandleEvent(EVENT_READ, 0);
		if ((mask & (FD_ADD_TRIAL_WRITE | FD_WRITE_WILL_BLOCK)) == FD_ADD_TRIAL_WRITE)
			eh->HandleEvent(EVENT_WRITE, 0);
	}
}

bool SocketEngine::HasFd(int fd)
{
	if ((fd < 0) || (fd > GetMaxFds()))
		return false;
	return (ref[fd] != NULL);
}

EventHandler* SocketEngine::GetRef(int fd)
{
	if ((fd < 0) || (fd > GetMaxFds()))
		return 0;
	return ref[fd];
}

bool SocketEngine::BoundsCheckFd(EventHandler* eh)
{
	if (!eh)
		return false;
	if ((eh->GetFd() < 0) || (eh->GetFd() > GetMaxFds()))
		return false;
	return true;
}


int SocketEngine::Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen)
{
	return accept(fd->GetFd(), addr, addrlen);
}

int SocketEngine::Close(EventHandler* fd)
{
#ifdef _WIN32
	return closesocket(fd->GetFd());
#else
	return close(fd->GetFd());
#endif
}

int SocketEngine::Close(int fd)
{
#ifdef _WIN32
	return closesocket(fd);
#else
	return close(fd);
#endif
}

int SocketEngine::Blocking(int fd)
{
#ifdef _WIN32
	unsigned long opt = 0;
	return ioctlsocket(fd, FIONBIO, &opt);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
}

int SocketEngine::NonBlocking(int fd)
{
#ifdef _WIN32
	unsigned long opt = 1;
	return ioctlsocket(fd, FIONBIO, &opt);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

void SocketEngine::SetReuse(int fd)
{
	int on = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));
}

int SocketEngine::RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen)
{
	int nbRecvd = recvfrom(fd->GetFd(), (char*)buf, len, flags, from, fromlen);
	if (nbRecvd > 0)
		this->UpdateStats(nbRecvd, 0);
	return nbRecvd;
}

int SocketEngine::Send(EventHandler* fd, const void *buf, size_t len, int flags)
{
	int nbSent = send(fd->GetFd(), (const char*)buf, len, flags);
	if (nbSent > 0)
		this->UpdateStats(0, nbSent);
	return nbSent;
}

int SocketEngine::Recv(EventHandler* fd, void *buf, size_t len, int flags)
{
	int nbRecvd = recv(fd->GetFd(), (char*)buf, len, flags);
	if (nbRecvd > 0)
		this->UpdateStats(nbRecvd, 0);
	return nbRecvd;
}

int SocketEngine::SendTo(EventHandler* fd, const void *buf, size_t len, int flags, const sockaddr *to, socklen_t tolen)
{
	int nbSent = sendto(fd->GetFd(), (const char*)buf, len, flags, to, tolen);
	if (nbSent > 0)
		this->UpdateStats(0, nbSent);
	return nbSent;
}

int SocketEngine::Connect(EventHandler* fd, const sockaddr *serv_addr, socklen_t addrlen)
{
	int ret = connect(fd->GetFd(), serv_addr, addrlen);
#ifdef _WIN32
	if ((ret == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK))
		errno = EINPROGRESS;
#endif
	return ret;
}

int SocketEngine::Shutdown(EventHandler* fd, int how)
{
	return shutdown(fd->GetFd(), how);
}

int SocketEngine::Bind(int fd, const irc::sockets::sockaddrs& addr)
{
	return bind(fd, &addr.sa, addr.sa_size());
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

std::string SocketEngine::LastError()
{
#ifndef _WIN32
	return strerror(errno);
#else
	char szErrorString[500];
	DWORD dwErrorCode = WSAGetLastError();
	if (FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)szErrorString, _countof(szErrorString), NULL) == 0)
		sprintf_s(szErrorString, _countof(szErrorString), "Error code: %u", dwErrorCode);

	std::string::size_type p;
	std::string ret = szErrorString;
	while ((p = ret.find_last_of("\r\n")) != std::string::npos)
		ret.erase(p, 1);

	return ret;
#endif
}

std::string SocketEngine::GetError(int errnum)
{
#ifndef _WIN32
	return strerror(errnum);
#else
	WSASetLastError(errnum);
	return LastError();
#endif
}
