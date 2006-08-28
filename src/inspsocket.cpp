/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "inspircd_config.h"
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
}

InspSocket::InspSocket(InspIRCd* SI, int newfd, const char* ip)
{
	this->fd = newfd;
	this->state = I_CONNECTED;
	strlcpy(this->IP,ip,MAXBUF);
	this->WaitingForWriteEvent = false;
	this->Instance = SI;
	if (this->fd > -1)
		this->Instance->SE->AddFd(this);
}

InspSocket::InspSocket(InspIRCd* SI, const std::string &ipaddr, int aport, bool listening, unsigned long maxtime)
{
	this->fd = -1;
	this->Instance = SI;
	strlcpy(host,ipaddr.c_str(),MAXBUF);
	this->WaitingForWriteEvent = false;
	if (listening)
	{
		if ((this->fd = OpenTCPSocket()) == ERROR)
		{
			this->fd = -1;
			this->state = I_ERROR;
			this->OnError(I_ERR_SOCKET);
			this->Instance->Log(DEBUG,"OpenTCPSocket() error");
			return;
		}
		else
		{
			if (!SI->BindSocket(this->fd,this->client,this->server,aport,(char*)ipaddr.c_str()))
			{
				this->Instance->Log(DEBUG,"BindSocket() error %s",strerror(errno));
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
				this->Instance->Log(DEBUG,"New socket now in I_LISTENING state");
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
			this->Instance->Log(DEBUG,"You cannot pass hostnames to InspSocket, resolve them first with Resolver!");
			this->Close();
			this->fd = -1;
			this->state = I_ERROR;
			this->OnError(I_ERR_RESOLVE);
			return;
		}
		else
		{
			this->Instance->Log(DEBUG,"No need to resolve %s",this->host);
			strlcpy(this->IP,host,MAXBUF);
			timeout_val = maxtime;
			this->DoConnect();
		}
	}
}

void InspSocket::WantWrite()
{
	/** XXX:
	 * The socket engine may only have each FD in the list ONCE.
	 * This means we cant watch for write AND read at the same
	 * time. We have to remove the READ fd, to insert the WRITE
	 * fd. Once we receive our WRITE event (which WILL ARRIVE,
	 * pretty much gauranteed) we switch back to watching for
	 * READ events again.
	 *
	 * This behaviour may be fixed in a later version.
	 */
	this->Instance->SE->DelFd(this);
	this->WaitingForWriteEvent = true;
	if (!this->Instance->SE->AddFd(this))
	{
		this->Close();
		this->fd = -1;
		this->state = I_ERROR;
		this->OnError(I_ERR_NOMOREFDS);
	}
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

	this->Instance->Log(DEBUG,"In InspSocket::BindAddr()");
	for (int j =0; j < Conf.Enumerate("bind"); j++)
	{
		std::string Type = Conf.ReadValue("bind","type",j);
		std::string IP = Conf.ReadValue("bind","address",j);
		if (Type == "servers")
		{
			if ((IP != "*") && (IP != "127.0.0.1") && (IP != ""))
			{
				insp_sockaddr s;

				if (insp_aton(IP.c_str(),&n) > 0)
				{
					this->Instance->Log(DEBUG,"Found an IP to bind to: %s",IP.c_str());
#ifdef IPV6
					s.sin6_addr = n;
					s.sin6_family = AF_FAMILY;
#else
					s.sin_addr = n;
					s.sin_family = AF_FAMILY;
#endif
					if (bind(this->fd,(struct sockaddr*)&s,sizeof(s)) < 0)
					{
						this->Instance->Log(DEBUG,"Cant bind()");
						this->state = I_ERROR;
						this->OnError(I_ERR_BIND);
						this->fd = -1;
						return false;
					}
					this->Instance->Log(DEBUG,"bind() reports outbound fd bound to ip %s",IP.c_str());
					return true;
				}
				else
				{
					this->Instance->Log(DEBUG,"Address '%s' was not an IP address",IP.c_str());
				}
			}
		}
	}
	this->Instance->Log(DEBUG,"Found no suitable IPs to bind, binding INADDR_ANY");
	return true;
}

bool InspSocket::DoConnect()
{
	this->Instance->Log(DEBUG,"In DoConnect()");
	if ((this->fd = socket(AF_FAMILY, SOCK_STREAM, 0)) == -1)
	{
		this->Instance->Log(DEBUG,"Cant socket()");
		this->state = I_ERROR;
		this->OnError(I_ERR_SOCKET);
		return false;
	}

	if ((strstr(this->IP,"::ffff:") != (char*)&this->IP) && (strstr(this->IP,"::FFFF:") != (char*)&this->IP))
	{
		if (!this->BindAddr())
			return false;
	}

	this->Instance->Log(DEBUG,"Part 2 DoConnect() %s",this->IP);
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
			this->Instance->Log(DEBUG,"Error connect() %d: %s",this->fd,strerror(errno));
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
	this->Instance->Log(DEBUG,"Returning true from InspSocket::DoConnect");
	return true;
}


void InspSocket::Close()
{
	if (this->fd > -1)
	{
		this->OnClose();
		shutdown(this->fd,2);
		close(this->fd);
	}
}

std::string InspSocket::GetIP()
{
	return this->IP;
}

char* InspSocket::Read()
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return NULL;
	int n = recv(this->fd,this->ibuf,sizeof(this->ibuf),0);
	if ((n > 0) && (n <= (int)sizeof(this->ibuf)))
	{
		ibuf[n] = 0;
		return ibuf;
	}
	else
	{
		int err = errno;
		if (err == EAGAIN)
		{
			return "";
		}
		else
		{
			if (!n)
				this->Instance->Log(DEBUG,"EOF or error on socket: EOF");
			else
				this->Instance->Log(DEBUG,"EOF or error on socket: %s",strerror(err));
			return NULL;
		}
	}
}

void InspSocket::MarkAsClosed()
{
	this->Instance->Log(DEBUG,"Marked as closed");
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
	return (!this->FlushWriteBuffer());
}

bool InspSocket::FlushWriteBuffer()
{
	if ((this->fd > -1) && (this->state == I_CONNECTED))
	{
		if (outbuffer.size())
		{
			int result = write(this->fd,outbuffer[0].c_str(),outbuffer[0].length());
			if (result > 0)
			{
				if ((unsigned int)result == outbuffer[0].length())
				{
					/* The whole block was written (usually a line)
					 * Pop the block off the front of the queue
					 */
					outbuffer.pop_front();
				}
				else
				{
					std::string temp = outbuffer[0].substr(result);
					outbuffer[0] = temp;
				}
			}
			else if ((result == -1) && (errno != EAGAIN))
			{
				this->Instance->Log(DEBUG,"Write error on socket: %s",strerror(errno));
				this->OnError(I_ERR_WRITE);
				this->state = I_ERROR;
				return true;
			}
		}
	}
	return (fd < 0);
}

void SocketTimeout::Tick(time_t now)
{
	if (ServerInstance->SE->GetRef(this->sfd) != this->sock)
	{
		ServerInstance->Log(DEBUG,"No FD or socket ref");
		return;
	}

	if (this->sock->state == I_CONNECTING)
	{
		ServerInstance->Log(DEBUG,"Timed out, current=%lu",now);
		// for non-listening sockets, the timeout can occur
		// which causes termination of the connection after
		// the given number of seconds without a successful
		// connection.
		this->sock->OnTimeout();
		this->sock->OnError(I_ERR_TIMEOUT);
		this->sock->timeout = true;
		this->sock->state = I_ERROR;
		ServerInstance->SE->DelFd(this->sock);
		this->sock->Close();
		delete this->sock;
		return;
	}
	this->sock->FlushWriteBuffer();
}

bool InspSocket::Poll()
{
	if (this->Instance->SE->GetRef(this->fd) != this)
		return false;

	int incoming = -1;
	bool n = true;

	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;

	switch (this->state)
	{
		case I_CONNECTING:
			this->Instance->Log(DEBUG,"State = I_CONNECTING");
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
			return this->OnConnected();
		break;
		case I_LISTENING:
			length = sizeof (client);
			incoming = accept (this->fd, (sockaddr*)&client,&length);
			this->SetQueues(incoming);
#ifdef IPV6
			this->OnIncomingConnection(incoming,(char*)insp_ntoa(client.sin6_addr));
#else
			this->OnIncomingConnection(incoming,(char*)insp_ntoa(client.sin_addr));
#endif
			return true;
		break;
		case I_CONNECTED:

			if (this->WaitingForWriteEvent)
			{
				/* Switch back to read events */
				this->Instance->SE->DelFd(this);
				this->WaitingForWriteEvent = false;
				if (!this->Instance->SE->AddFd(this))
					return false;

				/* Trigger the write event */
				n = this->OnWriteReady();
			}
			else
			{
				/* Process the read event */
				n = this->OnDataReady();
			}
			/* Flush any pending, but not till after theyre done with the event
			 * so there are less write calls involved.
			 * Both FlushWriteBuffer AND the return result of OnDataReady must
			 * return true for this to be ok.
			 */
			if (this->FlushWriteBuffer())
				return false;
			return n;
		break;
		default:
		break;
	}
	return true;
}

void InspSocket::SetState(InspSocketState s)
{
	this->Instance->Log(DEBUG,"Socket state change");
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

void InspSocket::HandleEvent(EventType et)
{
	if (!this->Poll())
	{
		this->Instance->SE->DelFd(this);
		this->Close();
		delete this;
	}
}

