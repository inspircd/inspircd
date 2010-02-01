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

#include "inspircd.h"
#include "socket.h"
#include "inspstring.h"
#include "socketengine.h"

#ifndef DISABLE_WRITEV
#include <sys/uio.h>
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif
#endif

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
		ServerInstance->SE->AddFd(this, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE);
}

void BufferedSocket::DoConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip)
{
	BufferedSocketError err = BeginConnect(ipaddr, aport, maxtime, connectbindip);
	if (err != I_ERR_NONE)
	{
		state = I_ERROR;
		SetError(strerror(errno));
		OnError(err);
	}
}

BufferedSocketError BufferedSocket::BeginConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip)
{
	irc::sockets::sockaddrs addr, bind;
	if (!irc::sockets::aptosa(ipaddr, aport, addr))
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "BUG: Hostname passed to BufferedSocket, rather than an IP address!");
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

static void IncreaseOSBuffers(int fd)
{
	// attempt to increase socket sendq and recvq as high as its possible
	int sendbuf = 32768;
	int recvbuf = 32768;
	setsockopt(fd,SOL_SOCKET,SO_SNDBUF,(const char *)&sendbuf,sizeof(sendbuf));
	setsockopt(fd,SOL_SOCKET,SO_RCVBUF,(const char *)&recvbuf,sizeof(recvbuf));
	// on failure, do nothing. I'm a little sick of people trying to interpret this message as a result of why their incorrect setups don't work.
}

BufferedSocketError BufferedSocket::BeginConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long timeout)
{
	if (fd < 0)
		fd = socket(dest.sa.sa_family, SOCK_STREAM, 0);

	if (fd < 0)
		return I_ERR_SOCKET;

	if (bind.sa.sa_family != 0)
	{
		if (ServerInstance->SE->Bind(fd, bind) < 0)
			return I_ERR_BIND;
	}

	ServerInstance->SE->NonBlocking(fd);

	if (ServerInstance->SE->Connect(this, &dest.sa, sa_size(dest)) == -1)
	{
		if (errno != EINPROGRESS)
			return I_ERR_CONNECT;
	}

	this->state = I_CONNECTING;

	if (!ServerInstance->SE->AddFd(this, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE))
		return I_ERR_NOMOREFDS;

	this->Timeout = new SocketTimeout(this->GetFd(), this, timeout, ServerInstance->Time());
	ServerInstance->Timers->AddTimer(this->Timeout);

	IncreaseOSBuffers(fd);

	ServerInstance->Logs->Log("SOCKET", DEBUG,"BufferedSocket::DoConnect success");
	return I_ERR_NONE;
}

void StreamSocket::Close()
{
	if (this->fd > -1)
	{
		if (IOHook)
		{
			try
			{
				IOHook->OnStreamSocketClose(this);
			}
			catch (CoreException& modexcept)
			{
				ServerInstance->Logs->Log("SOCKET", DEFAULT,"%s threw an exception: %s",
					modexcept.GetSource(), modexcept.GetReason());
			}
		}
		ServerInstance->SE->Shutdown(this, 2);
		ServerInstance->SE->DelFd(this);
		ServerInstance->SE->Close(this);
		fd = -1;
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
	line = recvq.substr(0, i);
	// TODO is this the most efficient way to split?
	recvq = recvq.substr(i + 1);
	return true;
}

void StreamSocket::DoRead()
{
	if (IOHook)
	{
		int rv = -1;
		try
		{
			rv = IOHook->OnStreamSocketRead(this, recvq);
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("SOCKET", DEFAULT, "%s threw an exception: %s",
				modexcept.GetSource(), modexcept.GetReason());
			return;
		}
		if (rv > 0)
			OnDataReady();
		if (rv < 0)
			SetError("Read Error"); // will not overwrite a better error message
	}
	else
	{
		char* ReadBuffer = ServerInstance->GetReadBuffer();
		int n = recv(fd, ReadBuffer, ServerInstance->Config->NetBufferSize, 0);
		if (n == ServerInstance->Config->NetBufferSize)
		{
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_READ | FD_ADD_TRIAL_READ);
			recvq.append(ReadBuffer, n);
			OnDataReady();
		}
		else if (n > 0)
		{
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_READ);
			recvq.append(ReadBuffer, n);
			OnDataReady();
		}
		else if (n == 0)
		{
			error = "Connection closed";
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		}
		else if (errno == EAGAIN)
		{
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_READ | FD_READ_WILL_BLOCK);
		}
		else if (errno == EINTR)
		{
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_READ | FD_ADD_TRIAL_READ);
		}
		else
		{
			error = strerror(errno);
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		}
	}
}

void StreamSocket::DoWrite()
{
	if (sendq.empty())
		return;
	if (!error.empty() || fd < 0 || fd == INT_MAX)
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "DoWrite on errored or closed socket");
		return;
	}

#ifndef DISABLE_WRITEV
	if (IOHook)
#endif
	{
		int rv = -1;
		try
		{
			while (error.empty() && !sendq.empty())
			{
				if (sendq.size() > 1 && sendq[0].length() < 1024)
				{
					// Avoid multiple repeated SSL encryption invocations
					// This adds a single copy of the queue, but avoids
					// much more overhead in terms of system calls invoked
					// by the IOHook.
					//
					// The length limit of 1024 is to prevent merging strings
					// more than once when writes begin to block.
					std::string tmp;
					tmp.reserve(sendq_len);
					for(unsigned int i=0; i < sendq.size(); i++)
						tmp.append(sendq[i]);
					sendq.clear();
					sendq.push_back(tmp);
				}
				std::string& front = sendq.front();
				int itemlen = front.length();
				if (IOHook)
				{
					rv = IOHook->OnStreamSocketWrite(this, front);
					if (rv > 0)
					{
						// consumed the entire string, and is ready for more
						sendq_len -= itemlen;
						sendq.pop_front();
					}
					else if (rv == 0)
					{
						// socket has blocked. Stop trying to send data.
						// IOHook has requested unblock notification from the socketengine

						// Since it is possible that a partial write took place, adjust sendq_len
						sendq_len = sendq_len - itemlen + front.length();
						return;
					}
					else
					{
						SetError("Write Error"); // will not overwrite a better error message
						return;
					}
				}
#ifdef DISABLE_WRITEV
				else
				{
					rv = ServerInstance->SE->Send(this, front.data(), itemlen, 0);
					if (rv == 0)
					{
						SetError("Connection closed");
						return;
					}
					else if (rv < 0)
					{
						if (errno == EAGAIN || errno == EINTR)
							ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK);
						else
							SetError(strerror(errno));
						return;
					}
					else if (rv < itemlen)
					{
						ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK);
						front = front.substr(itemlen - rv);
						sendq_len -= rv;
						return;
					}
					else
					{
						sendq_len -= itemlen;
						sendq.pop_front();
						if (sendq.empty())
							ServerInstance->SE->ChangeEventMask(this, FD_WANT_EDGE_WRITE);
					}
				}
#endif
			}
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("SOCKET", DEBUG,"%s threw an exception: %s",
				modexcept.GetSource(), modexcept.GetReason());
		}
	}
#ifndef DISABLE_WRITEV
	else
	{
		// don't even try if we are known to be blocking
		if (GetEventMask() & FD_WRITE_WILL_BLOCK)
			return;
		// start out optimistic - we won't need to write any more
		int eventChange = FD_WANT_EDGE_WRITE;
		while (error.empty() && sendq_len && eventChange == FD_WANT_EDGE_WRITE)
		{
			// Prepare a writev() call to write all buffers efficiently
			int bufcount = sendq.size();
		
			// cap the number of buffers at IOV_MAX
			if (bufcount > IOV_MAX)
			{
				bufcount = IOV_MAX;
			}

			int rv_max = 0;
			iovec* iovecs = new iovec[bufcount];
			for(int i=0; i < bufcount; i++)
			{
				iovecs[i].iov_base = const_cast<char*>(sendq[i].data());
				iovecs[i].iov_len = sendq[i].length();
				rv_max += sendq[i].length();
			}
			int rv = writev(fd, iovecs, bufcount);
			delete[] iovecs;

			if (rv == (int)sendq_len)
			{
				// it's our lucky day, everything got written out. Fast cleanup.
				// This won't ever happen if the number of buffers got capped.
				sendq_len = 0;
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
				sendq_len -= rv;
				while (rv > 0 && !sendq.empty())
				{
					std::string& front = sendq.front();
					if (front.length() <= (size_t)rv)
					{
						// this string got fully written out
						rv -= front.length();
						sendq.pop_front();
					}
					else
					{
						// stopped in the middle of this string
						front = front.substr(rv);
						rv = 0;
					}
				}
			}
			else if (rv == 0)
			{
				error = "Connection closed";
			}
			else if (errno == EAGAIN)
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
				error = strerror(errno);
			}
		}
		if (!error.empty())
		{
			// error - kill all events
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		}
		else
		{
			ServerInstance->SE->ChangeEventMask(this, eventChange);
		}
	}
#endif
}

void StreamSocket::WriteData(const std::string &data)
{
	if (fd < 0)
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Attempt to write data to dead socket: %s",
			data.c_str());
		return;
	}

	/* Append the data to the back of the queue ready for writing */
	sendq.push_back(data);
	sendq_len += data.length();

	ServerInstance->SE->ChangeEventMask(this, FD_ADD_TRIAL_WRITE);
}

void SocketTimeout::Tick(time_t)
{
	ServerInstance->Logs->Log("SOCKET", DEBUG,"SocketTimeout::Tick");

	if (ServerInstance->SE->GetRef(this->sfd) != this->sock)
		return;

	if (this->sock->state == I_CONNECTING)
	{
		// for connecting sockets, the timeout can occur
		// which causes termination of the connection after
		// the given number of seconds without a successful
		// connection.
		this->sock->OnTimeout();
		this->sock->OnError(I_ERR_TIMEOUT);

		/* NOTE: We must set this AFTER DelFd, as we added
		 * this socket whilst writeable. This means that we
		 * must DELETE the socket whilst writeable too!
		 */
		this->sock->state = I_ERROR;

		ServerInstance->GlobalCulls.AddItem(sock);
	}

	this->sock->Timeout = NULL;
}

void BufferedSocket::OnConnected() { }
void BufferedSocket::OnTimeout() { return; }

void BufferedSocket::DoWrite()
{
	if (state == I_CONNECTING)
	{
		state = I_CONNECTED;
		this->OnConnected();
		if (GetIOHook())
			GetIOHook()->OnStreamSocketConnect(this);
		else
			ServerInstance->SE->ChangeEventMask(this, FD_WANT_FAST_READ | FD_WANT_EDGE_WRITE);
	}
	this->StreamSocket::DoWrite();
}

BufferedSocket::~BufferedSocket()
{
	this->Close();
	if (Timeout)
	{
		ServerInstance->Timers->DelTimer(Timeout);
		Timeout = NULL;
	}
}

void StreamSocket::HandleEvent(EventType et, int errornum)
{
	if (!error.empty())
		return;
	BufferedSocketError errcode = I_ERR_OTHER;
	try {
		switch (et)
		{
			case EVENT_ERROR:
			{
				if (errornum == 0)
					SetError("Connection closed");
				else
					SetError(strerror(errornum));
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
				break;
			}
			case EVENT_READ:
			{
				DoRead();
				break;
			}
			case EVENT_WRITE:
			{
				DoWrite();
				break;
			}
		}
	}
	catch (CoreException& ex)
	{
		ServerInstance->Logs->Log("SOCKET", DEFAULT, "Caught exception in socket processing on FD %d - '%s'",
			fd, ex.GetReason());
		SetError(ex.GetReason());
	}
	if (!error.empty())
	{
		ServerInstance->Logs->Log("SOCKET", DEBUG, "Error on FD %d - '%s'", fd, error.c_str());
		OnError(errcode);
	}
}

