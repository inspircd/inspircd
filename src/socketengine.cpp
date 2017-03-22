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


/** Reference table, contains all current handlers
 **/
std::vector<EventHandler*> SocketEngine::ref;

/** Current number of descriptors in the engine
 */
size_t SocketEngine::CurrentSetSize = 0;

/** List of handlers that want a trial read/write
 */
std::set<int> SocketEngine::trials;

int SocketEngine::MAX_DESCRIPTORS;

/** Socket engine statistics: count of various events, bandwidth usage
 */
SocketEngine::Statistics SocketEngine::stats;

EventHandler::EventHandler()
{
	fd = -1;
	event_mask = 0;
}

void EventHandler::SetFd(int FD)
{
	this->fd = FD;
}

void EventHandler::OnEventHandlerWrite()
{
}

void EventHandler::OnEventHandlerError(int errornum)
{
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
			eh->OnEventHandlerRead();
		if ((mask & (FD_ADD_TRIAL_WRITE | FD_WRITE_WILL_BLOCK)) == FD_ADD_TRIAL_WRITE)
			eh->OnEventHandlerWrite();
	}
}

bool SocketEngine::AddFdRef(EventHandler* eh)
{
	int fd = eh->GetFd();
	if (HasFd(fd))
		return false;

	while (static_cast<unsigned int>(fd) >= ref.size())
		ref.resize(ref.empty() ? 1 : (ref.size() * 2));
	ref[fd] = eh;
	CurrentSetSize++;
	return true;
}

void SocketEngine::DelFdRef(EventHandler *eh)
{
	int fd = eh->GetFd();
	if (GetRef(fd) == eh)
	{
		ref[fd] = NULL;
		CurrentSetSize--;
	}
}

bool SocketEngine::HasFd(int fd)
{
	return GetRef(fd) != NULL;
}

EventHandler* SocketEngine::GetRef(int fd)
{
	if (fd < 0 || static_cast<unsigned int>(fd) >= ref.size())
		return NULL;
	return ref[fd];
}

bool SocketEngine::BoundsCheckFd(EventHandler* eh)
{
	if (!eh)
		return false;
	if (eh->GetFd() < 0)
		return false;
	return true;
}


int SocketEngine::Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen)
{
	return accept(fd->GetFd(), addr, addrlen);
}

int SocketEngine::Close(EventHandler* eh)
{
	DelFd(eh);
	int ret = Close(eh->GetFd());
	eh->SetFd(-1);
	return ret;
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
	stats.UpdateReadCounters(nbRecvd);
	return nbRecvd;
}

int SocketEngine::Send(EventHandler* fd, const void *buf, size_t len, int flags)
{
	int nbSent = send(fd->GetFd(), (const char*)buf, len, flags);
	stats.UpdateWriteCounters(nbSent);
	return nbSent;
}

int SocketEngine::Recv(EventHandler* fd, void *buf, size_t len, int flags)
{
	int nbRecvd = recv(fd->GetFd(), (char*)buf, len, flags);
	stats.UpdateReadCounters(nbRecvd);
	return nbRecvd;
}

int SocketEngine::SendTo(EventHandler* fd, const void *buf, size_t len, int flags, const sockaddr *to, socklen_t tolen)
{
	int nbSent = sendto(fd->GetFd(), (const char*)buf, len, flags, to, tolen);
	stats.UpdateWriteCounters(nbSent);
	return nbSent;
}

int SocketEngine::WriteV(EventHandler* fd, const IOVector* iovec, int count)
{
	int sent = writev(fd->GetFd(), iovec, count);
	stats.UpdateWriteCounters(sent);
	return sent;
}

#ifdef _WIN32
int SocketEngine::WriteV(EventHandler* fd, const iovec* iovec, int count)
{
	// On Windows the fields in iovec are not in the order required by the Winsock API; IOVector has
	// the fields in the correct order.
	// Create temporary IOVectors from the iovecs and pass them to the WriteV() method that accepts the
	// platform's native struct.
	IOVector wiovec[128];
	count = std::min(count, static_cast<int>(sizeof(wiovec) / sizeof(IOVector)));

	for (int i = 0; i < count; i++)
	{
		wiovec[i].iov_len = iovec[i].iov_len;
		wiovec[i].iov_base = reinterpret_cast<char*>(iovec[i].iov_base);
	}
	return WriteV(fd, wiovec, count);
}
#endif

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

void SocketEngine::Statistics::UpdateReadCounters(int len_in)
{
	CheckFlush();

	ReadEvents++;
	if (len_in > 0)
		indata += len_in;
	else if (len_in < 0)
		ErrorEvents++;
}

void SocketEngine::Statistics::UpdateWriteCounters(int len_out)
{
	CheckFlush();

	WriteEvents++;
	if (len_out > 0)
		outdata += len_out;
	else if (len_out < 0)
		ErrorEvents++;
}

void SocketEngine::Statistics::CheckFlush() const
{
	// Reset the in/out byte counters if it has been more than a second
	time_t now = ServerInstance->Time();
	if (lastempty != now)
	{
		lastempty = now;
		indata = outdata = 0;
	}
}

void SocketEngine::Statistics::GetBandwidth(float& kbitpersec_in, float& kbitpersec_out, float& kbitpersec_total) const
{
	CheckFlush();
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
