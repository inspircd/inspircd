/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

BufferedSocket::BufferedSocket()
{
	Timeout = NULL;
	state = I_ERROR;
}

BufferedSocket::BufferedSocket(int newfd)
{
	Timeout = NULL;
	this->fd = newfd;
	this->state = I_CONNECTED;
	if (fd > -1)
		SocketEngine::AddFd(this, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE);
}

void BufferedSocket::DoConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip)
{
	BufferedSocketError err = BeginConnect(ipaddr, aport, maxtime, connectbindip);
	if (err != I_ERR_NONE)
	{
		state = I_ERROR;
		SetError(SocketEngine::LastError());
		OnError(err);
	}
}

BufferedSocketError BufferedSocket::BeginConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip)
{
	irc::sockets::sockaddrs addr, bind;
	if (!irc::sockets::aptosa(ipaddr, aport, addr))
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "BUG: Hostname passed to BufferedSocket, rather than an IP address!");
		return I_ERR_CONNECT;
	}

	bind.sa.sa_family = 0;
	if (!connectbindip.empty())
	{
		if (!irc::sockets::aptosa(connectbindip, 0, bind))
		{
			return I_ERR_BIND;
		}
	}

	return BeginConnect(addr, bind, maxtime);
}

BufferedSocketError BufferedSocket::BeginConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long timeout)
{
	if (fd < 0)
		fd = socket(dest.sa.sa_family, SOCK_STREAM, 0);

	if (fd < 0)
		return I_ERR_SOCKET;

	if (bind.sa.sa_family != 0)
	{
		if (SocketEngine::Bind(fd, bind) < 0)
			return I_ERR_BIND;
	}

	SocketEngine::NonBlocking(fd);

	if (SocketEngine::Connect(this, &dest.sa, dest.sa_size()) == -1)
	{
		if (errno != EINPROGRESS)
			return I_ERR_CONNECT;
	}

	this->state = I_CONNECTING;

	if (!SocketEngine::AddFd(this, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE | FD_WRITE_WILL_BLOCK))
		return I_ERR_NOMOREFDS;

	this->Timeout = new SocketTimeout(this->GetFd(), this, timeout);
	ServerInstance->Timers.AddTimer(this->Timeout);

	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "BufferedSocket::DoConnect success");
	return I_ERR_NONE;
}

void StreamSocket::Close()
{
	if (this->fd > -1)
	{
		// final chance, dump as much of the sendq as we can
		DoWrite();
		if (GetIOHook())
		{
			GetIOHook()->OnStreamSocketClose(this);
			delete iohook;
			DelIOHook();
		}
		SocketEngine::Shutdown(this, 2);
		SocketEngine::Close(this);
	}
}

CullResult StreamSocket::cull()
{
	Close();
	return EventHandler::cull();
}

bool StreamSocket::GetNextLine(std::string& line, char delim)
{
	std::string::size_type i = recvq.find(delim);
	if (i == std::string::npos)
		return false;
	line.assign(recvq, 0, i);
	recvq.erase(0, i + 1);
	return true;
}

void StreamSocket::DoRead()
{
	if (GetIOHook())
	{
		int rv = GetIOHook()->OnStreamSocketRead(this, recvq);
		if (rv > 0)
			OnDataReady();
		if (rv < 0)
			SetError("Read Error"); // will not overwrite a better error message
	}
	else
	{
		char* ReadBuffer = ServerInstance->GetReadBuffer();
		int n = SocketEngine::Recv(this, ReadBuffer, ServerInstance->Config->NetBufferSize, 0);
		if (n == ServerInstance->Config->NetBufferSize)
		{
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_ADD_TRIAL_READ);
			recvq.append(ReadBuffer, n);
			OnDataReady();
		}
		else if (n > 0)
		{
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ);
			recvq.append(ReadBuffer, n);
			OnDataReady();
		}
		else if (n == 0)
		{
			error = "Connection closed";
			SocketEngine::ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		}
		else if (SocketEngine::IgnoreError())
		{
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_READ_WILL_BLOCK);
		}
		else if (errno == EINTR)
		{
			SocketEngine::ChangeEventMask(this, FD_WANT_FAST_READ | FD_ADD_TRIAL_READ);
		}
		else
		{
			error = SocketEngine::LastError();
			SocketEngine::ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		}
	}
}

/* Don't try to prepare huge blobs of data to send to a blocked socket */
static const int MYIOV_MAX = IOV_MAX < 128 ? IOV_MAX : 128;

void StreamSocket::DoWrite()
{
	if (sendq.empty())
		return;
	if (!error.empty() || fd < 0)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "DoWrite on errored or closed socket");
		return;
	}

	if (GetIOHook())
	{
		int rv = GetIOHook()->OnStreamSocketWrite(this, sendq);
		if (rv < 0)
			SetError("Write Error"); // will not overwrite a better error message

		// rv == 0 means the socket has blocked. Stop trying to send data.
		// IOHook has requested unblock notification from the socketengine.
	}
	else
	{
		// don't even try if we are known to be blocking
		if (GetEventMask() & FD_WRITE_WILL_BLOCK)
			return;
		// start out optimistic - we won't need to write any more
		int eventChange = FD_WANT_EDGE_WRITE;
		while (error.empty() && !sendq.empty() && eventChange == FD_WANT_EDGE_WRITE)
		{
			// Prepare a writev() call to write all buffers efficiently
			int bufcount = sendq.size();

			// cap the number of buffers at MYIOV_MAX
			if (bufcount > MYIOV_MAX)
			{
				bufcount = MYIOV_MAX;
			}

			int rv_max = 0;
			int rv;
			{
				SocketEngine::IOVector iovecs[MYIOV_MAX];
				size_t j = 0;
				for (SendQueue::const_iterator i = sendq.begin(), end = i+bufcount; i != end; ++i, j++)
				{
					const SendQueue::Element& elem = *i;
					iovecs[j].iov_base = const_cast<char*>(elem.data());
					iovecs[j].iov_len = elem.length();
					rv_max += elem.length();
				}
				rv = SocketEngine::WriteV(this, iovecs, bufcount);
			}

			if (rv == (int)sendq.bytes())
			{
				// it's our lucky day, everything got written out. Fast cleanup.
				// This won't ever happen if the number of buffers got capped.
				sendq.clear();
			}
			else if (rv > 0)
			{
				// Partial write. Clean out strings from the sendq
				if (rv < rv_max)
				{
					// it's going to block now
					eventChange = FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK;
				}
				while (rv > 0 && !sendq.empty())
				{
					const SendQueue::Element& front = sendq.front();
					if (front.length() <= (size_t)rv)
					{
						// this string got fully written out
						rv -= front.length();
						sendq.pop_front();
					}
					else
					{
						// stopped in the middle of this string
						sendq.erase_front(rv);
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
}

void StreamSocket::WriteData(const std::string &data)
{
	if (fd < 0)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Attempt to write data to dead socket: %s",
			data.c_str());
		return;
	}

	/* Append the data to the back of the queue ready for writing */
	sendq.push_back(data);

	SocketEngine::ChangeEventMask(this, FD_ADD_TRIAL_WRITE);
}

bool SocketTimeout::Tick(time_t)
{
	ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "SocketTimeout::Tick");

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

	this->sock->Timeout = NULL;
	delete this;
	return false;
}

void BufferedSocket::OnConnected() { }
void BufferedSocket::OnTimeout() { return; }

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
	catch (CoreException& ex)
	{
		ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT, "Caught exception in socket processing on FD %d - '%s'", fd, ex.GetReason().c_str());
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
		ServerInstance->Logs->Log("SOCKET", LOG_DEBUG, "Error on FD %d - '%s'", fd, error.c_str());
		OnError(errcode);
	}
}

IOHook* StreamSocket::GetModHook(Module* mod) const
{
	if (iohook)
	{
		if (iohook->prov->creator == mod)
			return iohook;
	}
	return NULL;
}
