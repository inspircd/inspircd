/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018, 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
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
#include "iohook.h"

static IOHook* GetNextHook(IOHook* hook)
{
	IOHookMiddle* const iohm = IOHookMiddle::ToMiddleHook(hook);
	if (iohm)
		return iohm->GetNextHook();
	return nullptr;
}

BufferedSocket::BufferedSocket()
	: state(I_ERROR)
{
}

BufferedSocket::BufferedSocket(int newfd)
	: state(I_CONNECTED)
{
	SetFd(newfd);
	if (HasFd())
		SocketEngine::AddFd(this, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE);
}

void BufferedSocket::DoConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long maxtime, int protocol)
{
	BufferedSocketError err = BeginConnect(dest, bind, maxtime, protocol);
	if (err != I_ERR_NONE)
	{
		state = I_ERROR;
		SetError(SocketEngine::LastError());
		OnError(err);
	}
}

BufferedSocketError BufferedSocket::BeginConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long timeout, int protocol)
{
	if (!HasFd())
		SetFd(socket(dest.family(), SOCK_STREAM, protocol));

	if (!HasFd())
		return I_ERR_SOCKET;

	if (bind.family() != 0)
	{
		if (SocketEngine::Bind(this, bind) < 0)
			return I_ERR_BIND;
	}

	SocketEngine::NonBlocking(GetFd());

	if (SocketEngine::Connect(this, dest) == -1)
	{
		if (errno != EINPROGRESS)
			return I_ERR_CONNECT;
	}

	this->state = I_CONNECTING;

	if (!SocketEngine::AddFd(this, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE | FD_WRITE_WILL_BLOCK))
		return I_ERR_NOMOREFDS;

	this->Timeout = new SocketTimeout(this->GetFd(), this, timeout);
	ServerInstance->Timers.AddTimer(this->Timeout);

	ServerInstance->Logs.Debug("SOCKET", "BufferedSocket::DoConnect success");
	return I_ERR_NONE;
}

void StreamSocket::Close()
{
	if (closing)
		return;

	closing = true;
	if (HasFd())
	{
		// final chance, dump as much of the sendq as we can
		DoWrite();

		IOHook* hook = GetIOHook();
		DelIOHook();
		while (hook)
		{
			hook->OnStreamSocketClose(this);
			IOHook* const nexthook = GetNextHook(hook);
			delete hook;
			hook = nexthook;
		}
		SocketEngine::Shutdown(this, 2);
		SocketEngine::Close(this);
	}
}

void StreamSocket::Close(bool writeblock)
{
	if (GetSendQSize() != 0 && writeblock)
		closeonempty = true;
	else
		Close();
}

Cullable::Result StreamSocket::Cull()
{
	Close();
	return EventHandler::Cull();
}

ssize_t StreamSocket::HookChainRead(IOHook* hook, std::string& rq)
{
	if (!hook)
		return ReadToRecvQ(rq);

	IOHookMiddle* const iohm = IOHookMiddle::ToMiddleHook(hook);
	if (iohm)
	{
		// Call the next hook to put data into the recvq of the current hook
		const ssize_t ret = HookChainRead(iohm->GetNextHook(), iohm->GetRecvQ());
		if (ret <= 0)
			return ret;
	}
	return hook->OnStreamSocketRead(this, rq);
}

void StreamSocket::DoRead()
{
	const std::string::size_type prevrecvqsize = recvq.size();

	const ssize_t result = HookChainRead(GetIOHook(), recvq);
	if (result < 0)
	{
		SetError("Read Error"); // will not overwrite a better error message
		return;
	}

	if (recvq.size() > prevrecvqsize)
		OnDataReady();
}

ssize_t StreamSocket::ReadToRecvQ(std::string& rq)
{
	char* ReadBuffer = ServerInstance->GetReadBuffer();
	ssize_t n = SocketEngine::Recv(this, ReadBuffer, ServerInstance->Config->NetBufferSize, 0);
	if (n >= 0)
	{
		size_t nrecv = static_cast<size_t>(n);
		if (nrecv == ServerInstance->Config->NetBufferSize)
		{
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_ADD_TRIAL_READ);
			rq.append(ReadBuffer, n);
		}
		else if (nrecv > 0)
		{
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ);
			rq.append(ReadBuffer, n);
		}
		else if (nrecv == 0)
		{
			error = "Connection closed";
			SocketEngine::ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
			return -1;
		}
	}
	else if (SocketEngine::IgnoreError())
	{
		SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_READ_WILL_BLOCK);
		return 0;
	}
	else if (errno == EINTR)
	{
		SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_ADD_TRIAL_READ);
		return 0;
	}
	else
	{
		error = SocketEngine::LastError();
		SocketEngine::ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		return -1;
	}
	return n;
}

/* Don't try to prepare huge blobs of data to send to a blocked socket */
static constexpr size_t MYIOV_MAX = std::min<size_t>(IOV_MAX, 128);

void StreamSocket::DoWrite()
{
	if (GetSendQSize() == 0)
	{
		if (closeonempty)
			Close();

		return;
	}
	if (!error.empty() || !HasFd())
	{
		ServerInstance->Logs.Debug("SOCKET", "DoWrite on errored or closed socket");
		return;
	}

	SendQueue* psendq = &sendq;
	IOHook* hook = GetIOHook();
	while (hook)
	{
		ssize_t rv = hook->OnStreamSocketWrite(this, *psendq);
		psendq = nullptr;

		// rv == 0 means the socket has blocked. Stop trying to send data.
		// IOHook has requested unblock notification from the socketengine.
		if (rv == 0)
			break;

		if (rv < 0)
		{
			SetError("Write Error"); // will not overwrite a better error message
			break;
		}

		IOHookMiddle* const iohm = IOHookMiddle::ToMiddleHook(hook);
		hook = nullptr;
		if (iohm)
		{
			psendq = &iohm->GetSendQ();
			hook = iohm->GetNextHook();
		}
	}

	if (psendq)
		FlushSendQ(*psendq);

	if (GetSendQSize() == 0 && closeonempty)
		Close();
}

void StreamSocket::FlushSendQ(SendQueue& sq)
{
		// don't even try if we are known to be blocking
		if (GetEventMask() & FD_WRITE_WILL_BLOCK)
			return;
		// start out optimistic - we won't need to write any more
		int eventChange = FD_WANT_EDGE_WRITE;
		while (error.empty() && !sq.empty() && eventChange == FD_WANT_EDGE_WRITE)
		{
			// Prepare a writev() call to write all buffers efficiently.
			int bufcount = static_cast<int>(std::min<size_t>(sq.size(), MYIOV_MAX));

			int rv_max = 0;
			ssize_t rv;
			{
				SocketEngine::IOVector iovecs[MYIOV_MAX];
				size_t j = 0;
				for (SendQueue::const_iterator i = sq.begin(), end = i+bufcount; i != end; ++i, j++)
				{
					const SendQueue::Element& elem = *i;
					iovecs[j].iov_base = const_cast<char*>(elem.data());
					iovecs[j].iov_len = elem.length();
					rv_max += iovecs[j].iov_len;
				}
				rv = SocketEngine::WriteV(this, iovecs, bufcount);
			}

			if (rv == static_cast<int>(sq.bytes()))
			{
				// it's our lucky day, everything got written out. Fast cleanup.
				// This won't ever happen if the number of buffers got capped.
				sq.clear();
			}
			else if (rv > 0)
			{
				// Partial write. Clean out strings from the sendq
				if (rv < rv_max)
				{
					// it's going to block now
					eventChange = FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK;
				}
				while (rv > 0 && !sq.empty())
				{
					const SendQueue::Element& front = sq.front();
					if (front.length() <= static_cast<size_t>(rv))
					{
						// this string got fully written out
						rv -= front.length();
						sq.pop_front();
					}
					else
					{
						// stopped in the middle of this string
						sq.erase_front(rv);
						rv = 0;
					}
				}
			}
			else if (rv == 0)
			{
				error = "Connection closed";
			}
			else if (SocketEngine::IgnoreError())
			{
				eventChange = FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK;
			}
			else if (errno == EINTR)
			{
				// restart interrupted syscall
				errno = 0;
			}
			else
			{
				error = SocketEngine::LastError();
			}
		}
		if (!error.empty())
		{
			// error - kill all events
			SocketEngine::ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		}
		else
		{
			SocketEngine::ChangeEventMask(this, eventChange);
		}
}

void StreamSocket::WriteData(const std::string& data)
{
	if (!HasFd())
	{
		ServerInstance->Logs.Debug("SOCKET", "Attempt to write data to dead socket: {}",
			data);
		return;
	}

	/* Append the data to the back of the queue ready for writing */
	sendq.push_back(data);

	SocketEngine::ChangeEventMask(this, FD_ADD_TRIAL_WRITE);
}

bool SocketTimeout::Tick()
{
	ServerInstance->Logs.Debug("SOCKET", "SocketTimeout::Tick");

	if (SocketEngine::GetRef(this->sfd) != this->sock)
	{
		delete this;
		return false;
	}

	if (this->sock->state == I_CONNECTING)
	{
		// for connecting sockets, the timeout can occur
		// which causes termination of the connection after
		// the given number of seconds without a successful
		// connection.
		this->sock->OnTimeout();
		this->sock->OnError(I_ERR_TIMEOUT);
		this->sock->state = I_ERROR;

		ServerInstance->GlobalCulls.AddItem(sock);
	}

	this->sock->Timeout = nullptr;
	delete this;
	return false;
}

void BufferedSocket::OnConnected() { }
void BufferedSocket::OnTimeout() { }

void BufferedSocket::OnEventHandlerWrite()
{
	if (state == I_CONNECTING)
	{
		state = I_CONNECTED;
		this->OnConnected();
		if (!GetIOHook())
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE);
	}
	this->StreamSocket::OnEventHandlerWrite();
}

BufferedSocket::~BufferedSocket()
{
	this->Close();
	// The timer is removed from the TimerManager in Timer::~Timer()
	delete Timeout;
}

void StreamSocket::OnEventHandlerError(int errornum)
{
	if (!error.empty())
		return;

	if (errornum == 0)
		SetError("Connection closed");
	else
		SetError(SocketEngine::GetError(errornum));

	BufferedSocketError errcode = I_ERR_OTHER;
	switch (errornum)
	{
		case ETIMEDOUT:
			errcode = I_ERR_TIMEOUT;
			break;
		case ECONNREFUSED:
		case 0:
			errcode = I_ERR_CONNECT;
			break;
		case EADDRINUSE:
			errcode = I_ERR_BIND;
			break;
		case EPIPE:
		case EIO:
			errcode = I_ERR_WRITE;
			break;
	}

	// Log and call OnError()
	CheckError(errcode);
}

void StreamSocket::OnEventHandlerRead()
{
	if (!error.empty())
		return;

	try
	{
		DoRead();
	}
	catch (const CoreException& ex)
	{
		ServerInstance->Logs.Normal("SOCKET", "Caught exception in socket processing on FD {} - '{}'",
			GetFd(), ex.GetReason());
		SetError(ex.GetReason());
	}
	CheckError(I_ERR_OTHER);
}

void StreamSocket::OnEventHandlerWrite()
{
	if (!error.empty())
		return;

	DoWrite();
	CheckError(I_ERR_OTHER);
}

void StreamSocket::CheckError(BufferedSocketError errcode)
{
	if (!error.empty())
	{
		ServerInstance->Logs.Debug("SOCKET", "Error on FD {} - '{}'", GetFd(), error);
		OnError(errcode);
	}
}

IOHook* StreamSocket::GetModHook(Module* mod) const
{
	for (IOHook* curr = GetIOHook(); curr; curr = GetNextHook(curr))
	{
		if (curr->prov->creator == mod)
			return curr;
	}
	return nullptr;
}

IOHook* StreamSocket::GetLastHook() const
{
	IOHook* curr = GetIOHook();
	IOHook* last = curr;

	for (; curr; curr = GetNextHook(curr))
		last = curr;

	return last;
}

void StreamSocket::AddIOHook(IOHook* newhook)
{
	IOHook* curr = GetIOHook();
	if (!curr)
	{
		iohook = newhook;
		return;
	}

	IOHookMiddle* lasthook = nullptr;
	while (curr)
	{
		lasthook = IOHookMiddle::ToMiddleHook(curr);
		if (!lasthook)
			return;
		curr = lasthook->GetNextHook();
	}

	lasthook->SetNextHook(newhook);
}

size_t StreamSocket::GetSendQSize() const
{
	size_t ret = sendq.bytes();
	IOHook* curr = GetIOHook();
	while (curr)
	{
		const IOHookMiddle* const iohm = IOHookMiddle::ToMiddleHook(curr);
		if (!iohm)
			break;

		ret += iohm->GetSendQ().bytes();
		curr = iohm->GetNextHook();
	}
	return ret;
}
