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

/* $Core */

#include "socket.h"
#include "inspstring.h"
#include "socketengine.h"
#include "inspircd.h"

bool BufferedSocket::Readable()
{
	return (this->state != I_CONNECTING);
}

BufferedSocket::BufferedSocket(InspIRCd* SI)
{
	this->Timeout = NULL;
	this->state = I_DISCONNECTED;
	this->fd = -1;
	this->ServerInstance = SI;
}

BufferedSocket::BufferedSocket(InspIRCd* SI, int newfd, const char* ip)
{
	this->Timeout = NULL;
	this->fd = newfd;
	this->state = I_CONNECTED;
	strlcpy(this->IP,ip,MAXBUF);
	this->ServerInstance = SI;
	if (this->fd > -1)
		this->ServerInstance->SE->AddFd(this);
}

BufferedSocket::BufferedSocket(InspIRCd* SI, const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip)
{
	this->cbindip = connectbindip;
	this->fd = -1;
	this->ServerInstance = SI;
	strlcpy(host,ipaddr.c_str(),MAXBUF);
	this->Timeout = NULL;

	strlcpy(this->host,ipaddr.c_str(),MAXBUF);
	this->port = aport;

	bool ipvalid = true;
#ifdef IPV6
	if (strchr(host,':'))
	{
		in6_addr n;
		if (inet_pton(AF_INET6, host, &n) < 1)
			ipvalid = false;
	}
	else
#endif
	{
		in_addr n;
		if (inet_aton(host,&n) < 1)
			ipvalid = false;
	}
	if (!ipvalid)
	{
		this->ServerInstance->Logs->Log("SOCKET", DEBUG,"BUG: Hostname passed to BufferedSocket, rather than an IP address!");
		this->OnError(I_ERR_CONNECT);
		this->Close();
		this->fd = -1;
		this->state = I_ERROR;
		return;
	}
	else
	{
		strlcpy(this->IP,host,MAXBUF);
		if (!this->DoConnect(maxtime))
		{
			this->OnError(I_ERR_CONNECT);
			this->Close();
			this->fd = -1;
			this->state = I_ERROR;
			return;
		}
	}
}

void BufferedSocket::SetQueues()
{
	// attempt to increase socket sendq and recvq as high as its possible
	int sendbuf = 32768;
	int recvbuf = 32768;
	if(setsockopt(this->fd,SOL_SOCKET,SO_SNDBUF,(const char *)&sendbuf,sizeof(sendbuf)) || setsockopt(this->fd,SOL_SOCKET,SO_RCVBUF,(const char *)&recvbuf,sizeof(sendbuf)))
	{
		//this->ServerInstance->Log(DEFAULT, "Could not increase SO_SNDBUF/SO_RCVBUF for socket %u", GetFd());
		; // do nothing. I'm a little sick of people trying to interpret this message as a result of why their incorrect setups don't work.
	}
}

bool BufferedSocket::DoBindMagic(const std::string &current_ip, bool v6)
{
	/* The [2] is required because we may write a sockaddr_in6 here, and sockaddr_in6 is larger than sockaddr, where sockaddr_in4 is not. */
	socklen_t size = sizeof(sockaddr_in);
	sockaddr* s = new sockaddr[2];
#ifdef IPV6
	if (v6)
	{
		in6_addr n;
		if (inet_pton(AF_INET6, current_ip.c_str(), &n) > 0)
		{
			memcpy(&((sockaddr_in6*)s)->sin6_addr, &n, sizeof(sockaddr_in6));
			((sockaddr_in6*)s)->sin6_port = 0;
			((sockaddr_in6*)s)->sin6_family = AF_INET6;
			size = sizeof(sockaddr_in6);
		}
		else
		{
			// Well, this is as good as it's gonna get.
			errno = EADDRNOTAVAIL;
			delete[] s;
			return false;
		}
	}
	else
#endif
	{
		in_addr n;
		if (inet_aton(current_ip.c_str(), &n) > 0)
		{
			((sockaddr_in*)s)->sin_addr = n;
			((sockaddr_in*)s)->sin_port = 0;
			((sockaddr_in*)s)->sin_family = AF_INET;
		}
		else
		{
			// Well, this is as good as it's gonna get.
			errno = EADDRNOTAVAIL;
			delete[] s;
			return false;
		}
	}

	if (ServerInstance->SE->Bind(this->fd, s, size) < 0)
	{
		this->state = I_ERROR;
		this->OnError(I_ERR_BIND);
		delete[] s;
		return false;
	}

	delete[] s;
	return true;
}

/* Most irc servers require you to specify the ip you want to bind to.
 * If you dont specify an IP, they rather dumbly bind to the first IP
 * of the box (e.g. INADDR_ANY). In InspIRCd, we scan thought the IP
 * addresses we've bound server ports to, and we try and bind our outbound
 * connections to the first usable non-loopback and non-any IP we find.
 * This is easier to configure when you have a lot of links and a lot
 * of servers to configure.
 */
bool BufferedSocket::BindAddr(const std::string &ip_to_bind)
{
	ConfigReader Conf(this->ServerInstance);
	bool v6 = false;

	// Case one: If they provided an IP, try bind it
	if (!ip_to_bind.empty())
	{
#ifdef IPV6
		// Check whether or not they are binding to an IPv6 IP..
		if (ip_to_bind.find(':') != std::string::npos)
			v6 = true;
#endif
		// And if it fails, don't do anything.
		return this->DoBindMagic(ip_to_bind, v6);
	}

	for (int j = 0; j < Conf.Enumerate("bind"); j++)
	{
		// We only want to try bind to a server ip.
		if (Conf.ReadValue("bind","type",j) != "servers")
			continue;

		// set current IP to the <bind> tag
		std::string current_ip = Conf.ReadValue("bind","address",j);

#ifdef IPV6
		// Check whether this <bind> is for an ipv6 address
		if (current_ip.find(':') != std::string::npos)
			v6 = true;
		else
			v6 = false;
#endif

		// Make sure IP is nothing local
		if (current_ip == "*" || current_ip == "127.0.0.1" || current_ip.empty() || current_ip == "::1")
			continue;

		// Try bind, don't fail if it doesn't bind though.
		if (this->DoBindMagic(current_ip, v6))
			return true;
	}

	// NOTE: You may wonder WTF we are returning *true* here, but that is because there were no custom binds setup, and so we have nothing to do
	// (remember, outgoing connections without binding are perfectly ok).
	ServerInstance->Logs->Log("SOCKET", DEBUG,"nothing in the config to bind()!");
	return true;
}

bool BufferedSocket::DoConnect(unsigned long maxtime)
{
	irc::sockets::sockaddrs addr;
	irc::sockets::aptosa(this->host, this->port, &addr);

	this->fd = socket(addr.sa.sa_family, SOCK_STREAM, 0);

	if (this->fd == -1)
	{
		this->state = I_ERROR;
		this->OnError(I_ERR_SOCKET);
		return false;
	}

	if (!this->BindAddr(this->cbindip))
	{
		this->Close();
		this->fd = -1;
		return false;
	}

	ServerInstance->SE->NonBlocking(this->fd);

	if (ServerInstance->SE->Connect(this, &addr.sa, sa_size(addr)) == -1)
	{
		if (errno != EINPROGRESS)
		{
			this->OnError(I_ERR_CONNECT);
			this->Close();
			this->state = I_ERROR;
			return false;
		}

		this->Timeout = new SocketTimeout(this->GetFd(), this->ServerInstance, this, maxtime, this->ServerInstance->Time());
		this->ServerInstance->Timers->AddTimer(this->Timeout);
	}

	this->state = I_CONNECTING;
	if (this->fd > -1)
	{
		if (!this->ServerInstance->SE->AddFd(this))
		{
			this->OnError(I_ERR_NOMOREFDS);
			this->Close();
			this->state = I_ERROR;
			return false;
		}
		this->SetQueues();
	}

	ServerInstance->Logs->Log("SOCKET", DEBUG,"BufferedSocket::DoConnect success");
	return true;
}


void BufferedSocket::Close()
{
	/* Save this, so we dont lose it,
	 * otherise on failure, error messages
	 * might be inaccurate.
	 */
	int save = errno;
	if (this->fd > -1)
	{
		if (this->GetIOHook())
		{
			try
			{
				this->GetIOHook()->OnRawSocketClose(this->fd);
			}
			catch (CoreException& modexcept)
			{
				ServerInstance->Logs->Log("SOCKET", DEFAULT,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			}
		}
		ServerInstance->SE->Shutdown(this, 2);
		ServerInstance->SE->DelFd(this, false);
		ServerInstance->SE->Close(this);
		this->OnClose();

		if (ServerInstance->SocketCull.find(this) == ServerInstance->SocketCull.end())
			ServerInstance->SocketCull[this] = this;
	}
	errno = save;
}

std::string BufferedSocket::GetIP()
{
	return this->IP;
}

const char* BufferedSocket::Read()
{
	if (!ServerInstance->SE->BoundsCheckFd(this))
		return NULL;

	int n = 0;
	char* ReadBuffer = ServerInstance->GetReadBuffer();

	if (this->GetIOHook())
	{
		int result2 = 0;
		int MOD_RESULT = 0;
		try
		{
			MOD_RESULT = this->GetIOHook()->OnRawSocketRead(this->fd, ReadBuffer, ServerInstance->Config->NetBufferSize, result2);
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("SOCKET", DEFAULT,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
		}
		if (MOD_RESULT < 0)
		{
			n = -1;
			errno = EAGAIN;
		}
		else
		{
			n = result2;
		}
	}
	else
	{
		n = recv(this->fd, ReadBuffer, ServerInstance->Config->NetBufferSize, 0);
	}

	/*
	 * This used to do some silly bounds checking instead of just passing bufsize - 1 to recv.
	 * Not only does that make absolutely no sense, but it could potentially result in a read buffer's worth
	 * of data being thrown into the bit bucket for no good reason, which is just *stupid*.. do things correctly now.
	 * --w00t (july 2, 2008)
	 */
	if (n > 0)
	{
		ReadBuffer[n] = 0;
		return ReadBuffer;
	}
	else
	{
		int err = errno;
		if (err == EAGAIN)
			return "";
		else
			return NULL;
	}
}

/*
 * This function formerly tried to flush write buffer each call.
 * While admirable in attempting to get the data out to wherever
 * it is going, on a full socket, it's just going to syscall write() and
 * EAGAIN constantly, instead of waiting in the SE to know if it can write
 * which will chew a bit of CPU.
 *
 * So, now this function returns void (take note) and just adds to the sendq.
 *
 * It'll get written at a determinate point when the socketengine tells us it can write.
 *		-- w00t (april 1, 2008)
 */
void BufferedSocket::Write(const std::string &data)
{
	/* Append the data to the back of the queue ready for writing */
	outbuffer.push_back(data);

	/* Mark ourselves as wanting write */
	this->ServerInstance->SE->WantWrite(this);
}

bool BufferedSocket::FlushWriteBuffer()
{
	errno = 0;
	if ((this->fd > -1) && (this->state == I_CONNECTED))
	{
		if (this->GetIOHook())
		{
			while (outbuffer.size() && (errno != EAGAIN))
			{
				try
				{
					/* XXX: The lack of buffering here is NOT a bug, modules implementing this interface have to
					 * implement their own buffering mechanisms
					 */
					this->GetIOHook()->OnRawSocketWrite(this->fd, outbuffer[0].c_str(), outbuffer[0].length());
					outbuffer.pop_front();
				}
				catch (CoreException& modexcept)
				{
					ServerInstance->Logs->Log("SOCKET", DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
					return true;
				}
			}
		}
		else
		{
			/* If we have multiple lines, try to send them all,
			 * not just the first one -- Brain
			 */
			while (outbuffer.size() && (errno != EAGAIN))
			{
				/* Send a line */
				int result = ServerInstance->SE->Send(this, outbuffer[0].c_str(), outbuffer[0].length(), 0);

				if (result > 0)
				{
					if ((unsigned int)result >= outbuffer[0].length())
					{
						/* The whole block was written (usually a line)
						 * Pop the block off the front of the queue,
						 * dont set errno, because we are clear of errors
						 * and want to try and write the next block too.
						 */
						outbuffer.pop_front();
					}
					else
					{
						std::string temp = outbuffer[0].substr(result);
						outbuffer[0] = temp;
						/* We didnt get the whole line out. arses.
						 * Try again next time, i guess. Set errno,
						 * because we shouldnt be writing any more now,
						 * until the socketengine says its safe to do so.
						 */
						errno = EAGAIN;
					}
				}
				else if (result == 0)
				{
					this->ServerInstance->SE->DelFd(this);
					this->Close();
					return true;
				}
				else if ((result == -1) && (errno != EAGAIN))
				{
					this->OnError(I_ERR_WRITE);
					this->state = I_ERROR;
					this->ServerInstance->SE->DelFd(this);
					this->Close();
					return true;
				}
			}
		}
	}

	if ((errno == EAGAIN) && (fd > -1))
	{
		this->ServerInstance->SE->WantWrite(this);
	}

	return (fd < 0);
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

		if (ServerInstance->SocketCull.find(this->sock) == ServerInstance->SocketCull.end())
			ServerInstance->SocketCull[this->sock] = this->sock;
	}

	this->sock->Timeout = NULL;
}

bool BufferedSocket::InternalMarkConnected()
{
	/* Our socket was in write-state, so delete it and re-add it
	 * in read-state.
	 */
	this->SetState(I_CONNECTED);

	if (this->GetIOHook())
	{
		ServerInstance->Logs->Log("SOCKET",DEBUG,"Hook for raw connect");
		try
		{
			this->GetIOHook()->OnRawSocketConnect(this->fd);
		}
		catch (CoreException& modexcept)
		{
			ServerInstance->Logs->Log("SOCKET",DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			return false;
		}
	}
	return this->OnConnected();
}

void BufferedSocket::SetState(BufferedSocketState s)
{
	this->state = s;
}

BufferedSocketState BufferedSocket::GetState()
{
	return this->state;
}

bool BufferedSocket::OnConnected() { return true; }
void BufferedSocket::OnError(BufferedSocketError) { return; }
int BufferedSocket::OnDisconnect() { return 0; }
bool BufferedSocket::OnDataReady() { return true; }
bool BufferedSocket::OnWriteReady()
{
	// Default behaviour: just try write some.
	return !this->FlushWriteBuffer();
}
void BufferedSocket::OnTimeout() { return; }
void BufferedSocket::OnClose() { return; }

BufferedSocket::~BufferedSocket()
{
	this->Close();
	if (Timeout)
	{
		ServerInstance->Timers->DelTimer(Timeout);
		Timeout = NULL;
	}
}

void BufferedSocket::HandleEvent(EventType et, int errornum)
{
	switch (et)
	{
		case EVENT_ERROR:
		{
			switch (errornum)
			{
				case ETIMEDOUT:
					this->OnError(I_ERR_TIMEOUT);
					break;
				case ECONNREFUSED:
				case 0:
					this->OnError(this->state == I_CONNECTING ? I_ERR_CONNECT : I_ERR_WRITE);
					break;
				case EADDRINUSE:
					this->OnError(I_ERR_BIND);
					break;
				case EPIPE:
				case EIO:
					this->OnError(I_ERR_WRITE);
					break;
			}

			if (this->ServerInstance->SocketCull.find(this) == this->ServerInstance->SocketCull.end())
				this->ServerInstance->SocketCull[this] = this;
			return;
			break;
		}
		case EVENT_READ:
		{
			if (!this->OnDataReady())
			{
				if (this->ServerInstance->SocketCull.find(this) == this->ServerInstance->SocketCull.end())
					this->ServerInstance->SocketCull[this] = this;
				return;
			}
			break;
		}
		case EVENT_WRITE:
		{
			if (this->state == I_CONNECTING)
			{
				if (!this->InternalMarkConnected())
				{
					if (this->ServerInstance->SocketCull.find(this) == this->ServerInstance->SocketCull.end())
						this->ServerInstance->SocketCull[this] = this;
					return;
				}
				return;
			}
			else
			{
				if (!this->OnWriteReady())
				{
					if (this->ServerInstance->SocketCull.find(this) == this->ServerInstance->SocketCull.end())
						this->ServerInstance->SocketCull[this] = this;
					return;
				}
			}
			break;
		}
	}
}

