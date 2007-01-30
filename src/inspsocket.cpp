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

#include "socket.h"
#include "configreader.h"
#include "inspstring.h"
#include "socketengine.h"
#include "inspircd.h"

using irc::sockets::OpenTCPSocket;
using irc::sockets::insp_inaddr;
using irc::sockets::insp_sockaddr;

bool InspSocket::Readable()
{
	return ((this->state != I_CONNECTING) && (this->WaitingForWriteEvent == false));
}

InspSocket::InspSocket(InspIRCd* SI)
{
	this->state = I_DISCONNECTED;
	this->fd = -1;
	this->WaitingForWriteEvent = false;
	this->Instance = SI;
	this->IsIOHooked = false;
}

InspSocket::InspSocket(InspIRCd* SI, int newfd, const char* ip)
{
	this->fd = newfd;
	this->state = I_CONNECTED;
	strlcpy(this->IP,ip,MAXBUF);
	this->WaitingForWriteEvent = false;
	this->Instance = SI;
	this->IsIOHooked = false;
	if (this->fd > -1)
		this->Instance->SE->AddFd(this);
}

InspSocket::InspSocket(InspIRCd* SI, const std::string &ipaddr, int aport, bool listening, unsigned long maxtime)
{
	this->fd = -1;
	this->Instance = SI;
	strlcpy(host,ipaddr.c_str(),MAXBUF);
	this->WaitingForWriteEvent = false;
	this->IsIOHooked = false;
	if (listening)
	{
		if ((this->fd = OpenTCPSocket()) == ERROR)
		{
			this->fd = -1;
			this->state = I_ERROR;
			this->OnError(I_ERR_SOCKET);
			return;
		}
		else
		{
			if (!SI->BindSocket(this->fd,this->client,this->server,aport,(char*)ipaddr.c_str()))
			{
				this->Close();
				this->fd = -1;
				this->state = I_ERROR;
				this->OnError(I_ERR_BIND);
				this->ClosePending = true;
				return;
			}
			else
			{
				this->state = I_LISTENING;
				if (this->fd > -1)
				{
					if (!this->Instance->SE->AddFd(this))
					{
						this->Close();
						this->state = I_ERROR;
						this->OnError(I_ERR_NOMOREFDS);
					}
				}
				return;
			}
		}			
	}
	else
	{
		strlcpy(this->host,ipaddr.c_str(),MAXBUF);
		this->port = aport;

		if (insp_aton(host,&addy) < 1)
		{
			this->Instance->Log(DEBUG,"BUG: Hostname passed to InspSocket, rather than an IP address!");
			this->Close();
			this->fd = -1;
			this->state = I_ERROR;
			this->OnError(I_ERR_RESOLVE);
			return;
		}
		else
		{
			strlcpy(this->IP,host,MAXBUF);
			timeout_val = maxtime;
			this->DoConnect();
		}
	}
}

void InspSocket::WantWrite()
{
	this->Instance->SE->WantWrite(this);
	this->WaitingForWriteEvent = true;
}

void InspSocket::SetQueues(int nfd)
{
	// attempt to increase socket sendq and recvq as high as its possible
	int sendbuf = 32768;
	int recvbuf = 32768;
	setsockopt(nfd,SOL_SOCKET,SO_SNDBUF,(const void *)&sendbuf,sizeof(sendbuf));
	setsockopt(nfd,SOL_SOCKET,SO_RCVBUF,(const void *)&recvbuf,sizeof(sendbuf));
}

/* Most irc servers require you to specify the ip you want to bind to.
 * If you dont specify an IP, they rather dumbly bind to the first IP
 * of the box (e.g. INADDR_ANY). In InspIRCd, we scan thought the IP
 * addresses we've bound server ports to, and we try and bind our outbound
 * connections to the first usable non-loopback and non-any IP we find.
 * This is easier to configure when you have a lot of links and a lot
 * of servers to configure.
 */
bool InspSocket::BindAddr()
{
	insp_inaddr n;
	ConfigReader Conf(this->Instance);
	for (int j =0; j < Conf.Enumerate("bind"); j++)
	{
		std::string Type = Conf.ReadValue("bind","type",j);
		std::string IP = Conf.ReadValue("bind","address",j);
		if (Type == "servers")
		{
			if ((IP != "*") && (IP != "127.0.0.1") && (IP != "") && (IP != "::1"))
			{
				insp_sockaddr s;

				if (insp_aton(IP.c_str(),&n) > 0)
				{
#ifdef IPV6
					s.sin6_addr = n;
					s.sin6_family = AF_FAMILY;
#else
					s.sin_addr = n;
					s.sin_family = AF_FAMILY;
#endif
					if (bind(this->fd,(struct sockaddr*)&s,sizeof(s)) < 0)
					{
						this->state = I_ERROR;
						this->OnError(I_ERR_BIND);
						this->fd = -1;
						return false;
					}
					return true;
				}
			}
		}
	}
	return true;
}

bool InspSocket::DoConnect()
{
	if ((this->fd = socket(AF_FAMILY, SOCK_STREAM, 0)) == -1)
	{
		this->state = I_ERROR;
		this->OnError(I_ERR_SOCKET);
		return false;
	}

	if ((strstr(this->IP,"::ffff:") != (char*)&this->IP) && (strstr(this->IP,"::FFFF:") != (char*)&this->IP))
	{
		if (!this->BindAddr())
			return false;
	}

	insp_aton(this->IP,&addy);
#ifdef IPV6
	addr.sin6_family = AF_FAMILY;
	memcpy(&addr.sin6_addr, &addy, sizeof(insp_inaddr));
	addr.sin6_port = htons(this->port);
#else
	addr.sin_family = AF_FAMILY;
	addr.sin_addr = addy;
	addr.sin_port = htons(this->port);
#endif

	int flags;
	flags = fcntl(this->fd, F_GETFL, 0);
	fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);

	if (connect(this->fd, (sockaddr*)&this->addr,sizeof(this->addr)) == -1)
	{
		if (errno != EINPROGRESS)
		{
			this->OnError(I_ERR_CONNECT);
			this->Close();
			this->state = I_ERROR;
			return false;
		}

		this->Timeout = new SocketTimeout(this->GetFd(), this->Instance, this, timeout_val, this->Instance->Time());
		this->Instance->Timers->AddTimer(this->Timeout);
	}
	this->state = I_CONNECTING;
	if (this->fd > -1)
	{
		if (!this->Instance->SE->AddFd(this))
		{
			this->OnError(I_ERR_NOMOREFDS);
			this->Close();
			this->state = I_ERROR;
			return false;
		}
		this->SetQueues(this->fd);
	}
	return true;
}


void InspSocket::Close()
{
	/* Save this, so we dont lose it,
	 * otherise on failure, error messages
	 * might be inaccurate.
	 */
	int save = errno;
	if (this->fd > -1)
	{
                if (this->IsIOHooked && Instance->Config->GetIOHook(this))
		{
			try
			{
				Instance->Config->GetIOHook(this)->OnRawSocketClose(this->fd);
			}
			catch (CoreException& modexcept)
			{
				Instance->Log(DEFAULT,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
			}
		}
		this->OnClose();
		shutdown(this->fd,2);
		close(this->fd);
	}
	errno = save;
}

std::string InspSocket::GetIP()
{
	return this->IP;
}

char* InspSocket::Read()
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return NULL;

	int n = 0;

	if (this->IsIOHooked)
	{
		int result2 = 0;
		int MOD_RESULT = 0;
		try
		{
			MOD_RESULT = Instance->Config->GetIOHook(this)->OnRawSocketRead(this->fd,this->ibuf,sizeof(this->ibuf),result2);
		}
		catch (CoreException& modexcept)
		{
			Instance->Log(DEFAULT,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
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
		n = recv(this->fd,this->ibuf,sizeof(this->ibuf),0);
	}

	if ((n > 0) && (n <= (int)sizeof(this->ibuf)))
	{
		ibuf[n] = 0;
		return ibuf;
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

void InspSocket::MarkAsClosed()
{
}

// There are two possible outcomes to this function.
// It will either write all of the data, or an undefined amount.
// If an undefined amount is written the connection has failed
// and should be aborted.
int InspSocket::Write(const std::string &data)
{
	/* Try and append the data to the back of the queue, and send it on its way
	 */
	outbuffer.push_back(data);
	this->Instance->SE->WantWrite(this);
	return (!this->FlushWriteBuffer());
}

bool InspSocket::FlushWriteBuffer()
{
	errno = 0;
	if ((this->fd > -1) && (this->state == I_CONNECTED))
	{
		if (this->IsIOHooked)
		{
			while (outbuffer.size() && (errno != EAGAIN))
			{
				try
				{
					int result = Instance->Config->GetIOHook(this)->OnRawSocketWrite(this->fd, outbuffer[0].c_str(), outbuffer[0].length());
					if (result > 0)
					{
						if ((unsigned int)result == outbuffer[0].length())
						{
							outbuffer.pop_front();
						}
						else
						{
							std::string temp = outbuffer[0].substr(result);
							outbuffer[0] = temp;
							errno = EAGAIN;
						}
					}
					else if (((result == -1) && (errno != EAGAIN)) || (result == 0))
					{
						this->OnError(I_ERR_WRITE);
						this->state = I_ERROR;
						this->Instance->SE->DelFd(this);
						this->Close();
						return true;
					}
				}
				catch (CoreException& modexcept)
				{
					Instance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
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
				int result = write(this->fd,outbuffer[0].c_str(),outbuffer[0].length());
				if (result > 0)
				{
					if ((unsigned int)result == outbuffer[0].length())
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
				else if ((result == -1) && (errno != EAGAIN))
				{
					this->OnError(I_ERR_WRITE);
					this->state = I_ERROR;
					this->Instance->SE->DelFd(this);
					this->Close();
					return true;
				}
			}
		}
	}

	if ((errno == EAGAIN) && (fd > -1))
	{
		this->Instance->SE->WantWrite(this);
	}

	return (fd < 0);
}

void SocketTimeout::Tick(time_t now)
{
	if (ServerInstance->SE->GetRef(this->sfd) != this->sock)
		return;

	if (this->sock->state == I_CONNECTING)
	{
		// for non-listening sockets, the timeout can occur
		// which causes termination of the connection after
		// the given number of seconds without a successful
		// connection.
		this->sock->OnTimeout();
		this->sock->OnError(I_ERR_TIMEOUT);
		this->sock->timeout = true;
		ServerInstance->SE->DelFd(this->sock);
		/* NOTE: We must set this AFTER DelFd, as we added
		 * this socket whilst writeable. This means that we
		 * must DELETE the socket whilst writeable too!
		 */
		this->sock->state = I_ERROR;
		this->sock->Close();
		delete this->sock;
		return;
	}
}

bool InspSocket::Poll()
{
	if (this->Instance->SE->GetRef(this->fd) != this)
		return false;

	int incoming = -1;

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	switch (this->state)
	{
		case I_CONNECTING:
			/* Our socket was in write-state, so delete it and re-add it
			 * in read-state.
			 */
			if (this->fd > -1)
			{
				this->Instance->SE->DelFd(this);
				this->SetState(I_CONNECTED);
				if (!this->Instance->SE->AddFd(this))
					return false;
			}
			if (Instance->Config->GetIOHook(this))
			{
				try
				{
					Instance->Config->GetIOHook(this)->OnRawSocketConnect(this->fd);
				}
				catch (CoreException& modexcept)
				{
					Instance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}
			return this->OnConnected();
		break;
		case I_LISTENING:
			length = sizeof (client);
			incoming = accept (this->fd, (sockaddr*)&client,&length);

#ifdef IPV6
			this->OnIncomingConnection(incoming, (char*)insp_ntoa(client.sin6_addr));
#else
			this->OnIncomingConnection(incoming, (char*)insp_ntoa(client.sin_addr));
#endif

			if (this->IsIOHooked)
			{
				try
				{
#ifdef IPV6
					Instance->Config->GetIOHook(this)->OnRawSocketAccept(incoming, insp_ntoa(client.sin6_addr), this->port);
#else
					Instance->Config->GetIOHook(this)->OnRawSocketAccept(incoming, insp_ntoa(client.sin_addr), this->port);
#endif
				}
				catch (CoreException& modexcept)
				{
					Instance->Log(DEBUG,"%s threw an exception: %s", modexcept.GetSource(), modexcept.GetReason());
				}
			}

			this->SetQueues(incoming);
			return true;
		break;
		case I_CONNECTED:
			/* Process the read event */
			return this->OnDataReady();
		break;
		default:
		break;
	}
	return true;
}

void InspSocket::SetState(InspSocketState s)
{
	this->state = s;
}

InspSocketState InspSocket::GetState()
{
	return this->state;
}

int InspSocket::GetFd()
{
	return this->fd;
}

bool InspSocket::OnConnected() { return true; }
void InspSocket::OnError(InspSocketError e) { return; }
int InspSocket::OnDisconnect() { return 0; }
int InspSocket::OnIncomingConnection(int newfd, char* ip) { return 0; }
bool InspSocket::OnDataReady() { return true; }
bool InspSocket::OnWriteReady() { return true; }
void InspSocket::OnTimeout() { return; }
void InspSocket::OnClose() { return; }

InspSocket::~InspSocket()
{
	this->Close();
}

void InspSocket::HandleEvent(EventType et, int errornum)
{
	switch (et)
	{
		case EVENT_ERROR:
			this->Instance->SE->DelFd(this);
			this->Close();
			delete this;
			return;
		break;
		case EVENT_READ:
			if (!this->Poll())
			{
				this->Instance->SE->DelFd(this);
				this->Close();
				delete this;
				return;
			}
		break;
		case EVENT_WRITE:
			if (this->WaitingForWriteEvent)
			{
				this->WaitingForWriteEvent = false;
				if (!this->OnWriteReady())
				{
					this->Instance->SE->DelFd(this);
					this->Close();
					delete this;
					return;
				}
			}
			if (this->state == I_CONNECTING)
			{
				/* This might look wrong as if we should be actually calling
				 * with EVENT_WRITE, but trust me it is correct. There are some
				 * writeability-state things in the read code, because of how
				 * InspSocket used to work regarding write buffering in previous
				 * versions of InspIRCd. - Brain
				 */
				this->HandleEvent(EVENT_READ);
				return;
			}
			else
			{
				if (this->FlushWriteBuffer())
				{
					this->Instance->SE->DelFd(this);
					this->Close();
					delete this;
					return;
				}
			}
		break;
	}
}

