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

using namespace std;

#include "inspircd_config.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include "socket.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "socketengine.h"


extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern time_t TIME;

InspSocket* socket_ref[MAX_DESCRIPTORS];

InspSocket::InspSocket()
{
	this->state = I_DISCONNECTED;
	this->fd = -1;
}

InspSocket::InspSocket(int newfd, char* ip)
{
	this->fd = newfd;
	this->state = I_CONNECTED;
	this->IP = ip;
	ServerInstance->SE->AddFd(this->fd,true,X_ESTAB_MODULE);
	socket_ref[this->fd] = this;
}

InspSocket::InspSocket(std::string ahost, int aport, bool listening, unsigned long maxtime)
{
	this->fd = -1;
	this->host = ahost;
	this->Buffer = "";
	if (listening) {
		if ((this->fd = OpenTCPSocket()) == ERROR)
		{
			this->fd = -1;
			this->state = I_ERROR;
			this->OnError(I_ERR_SOCKET);
			log(DEBUG,"OpenTCPSocket() error");
                        return;
		}
		else
		{
			if (BindSocket(this->fd,this->client,this->server,aport,(char*)ahost.c_str()) == ERROR)
			{
				this->Close();
				this->fd = -1;
				this->state = I_ERROR;
				this->OnError(I_ERR_BIND);
				log(DEBUG,"BindSocket() error %s",strerror(errno));
				return;
			}
			else
			{
				this->state = I_LISTENING;
				ServerInstance->SE->AddFd(this->fd,true,X_ESTAB_MODULE);
				socket_ref[this->fd] = this;
				log(DEBUG,"New socket now in I_LISTENING state");
				return;
			}
		}			
	}
	else
	{
		this->host = ahost;
		this->port = aport;

		if (!inet_aton(host.c_str(),&addy))
		{
			log(DEBUG,"Attempting to resolve %s",this->host.c_str());
			/* Its not an ip, spawn the resolver */
			this->dns.SetNS(std::string(Config->DNSServer));
			this->dns.ForwardLookupWithFD(host,fd);
			timeout_end = time(NULL) + maxtime;
	                timeout = false;
			this->state = I_RESOLVING;
			socket_ref[this->fd] = this;
		}
		else
		{
			log(DEBUG,"No need to resolve %s",this->host.c_str());
			this->IP = host;
			timeout_end = time(NULL) + maxtime;
			this->DoConnect();
		}
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

bool InspSocket::DoResolve()
{
	log(DEBUG,"In DoResolve(), trying to resolve IP");
	if (this->dns.HasResult())
	{
		log(DEBUG,"Socket has result");
		std::string res_ip = dns.GetResultIP();
		if (res_ip != "")
		{
			log(DEBUG,"Socket result set to %s",res_ip.c_str());
			this->IP = res_ip;
			socket_ref[this->fd] = NULL;
		}
		else
		{
			log(DEBUG,"Socket DNS failure");
			this->Close();
			this->state = I_ERROR;
			this->OnError(I_ERR_RESOLVE);
			return false;
		}
		return this->DoConnect();
	}
	log(DEBUG,"No result for socket yet!");
	return true;
}

bool InspSocket::DoConnect()
{
	log(DEBUG,"In DoConnect()");
	if ((this->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		log(DEBUG,"Cant socket()");
		this->state = I_ERROR;
		this->OnError(I_ERR_SOCKET);
		return false;
	}

	log(DEBUG,"Part 2 DoConnect() %s",this->IP.c_str());
	inet_aton(this->IP.c_str(),&addy);
	addr.sin_family = AF_INET;
	addr.sin_addr = addy;
	addr.sin_port = htons(this->port);

	int flags;
	flags = fcntl(this->fd, F_GETFL, 0);
	fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);

	if (connect(this->fd, (sockaddr*)&this->addr,sizeof(this->addr)) == -1)
	{
		if (errno != EINPROGRESS)
		{
			log(DEBUG,"Error connect() %d: %s",this->fd,strerror(errno));
			this->OnError(I_ERR_CONNECT);
			this->state = I_ERROR;
			this->Close();
			return false;
		}
	}
	this->state = I_CONNECTING;
	ServerInstance->SE->AddFd(this->fd,false,X_ESTAB_MODULE);
	socket_ref[this->fd] = this;
	this->SetQueues(this->fd);
	log(DEBUG,"Returning true from InspSocket::DoConnect");
	return true;
}


void InspSocket::Close()
{
	if (this->fd != -1)
	{
		this->OnClose();
	        shutdown(this->fd,2);
	        close(this->fd);
		socket_ref[this->fd] = NULL;
	        this->fd = -1;
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
		if (errno == EAGAIN)
		{
			return "";
		}
		else
		{
			log(DEBUG,"EOF or error on socket");
			return NULL;
		}
	}
}

// There are two possible outcomes to this function.
// It will either write all of the data, or an undefined amount.
// If an undefined amount is written the connection has failed
// and should be aborted.
int InspSocket::Write(std::string data)
{
	try
	{
		if ((data != "") && (this->Buffer.length() + data.length() < this->Buffer.max_size()))
			this->Buffer.append(data);
	}
	catch (std::length_error)
	{
		log(DEBUG,"std::length_error exception caught while appending to socket buffer!");
		return 0;
	}
	return data.length();
}

void InspSocket::FlushWriteBuffer()
{
	if ((this->fd > -1) && (this->state == I_CONNECTED))
	{
		int result = 0, v = 0;
		const char* n = Buffer.c_str();
		v = Buffer.length();
		if (v > 0)
		{
			result = write(this->fd,n,v);
			if (result > 0)
			{
				if (result == v)
				{
					Buffer = "";
				}
				else
				{
					/* If we wrote some, advance the buffer forwards */
					n += result;
					Buffer = n;
				}
			}
		}
	}
}

bool InspSocket::Timeout(time_t current)
{
	if (((this->state == I_RESOLVING) || (this->state == I_CONNECTING)) && (current > timeout_end))
	{
		log(DEBUG,"Timed out, current=%lu timeout_end=%lu");
		// for non-listening sockets, the timeout can occur
		// which causes termination of the connection after
		// the given number of seconds without a successful
		// connection.
		this->OnTimeout();
		this->OnError(I_ERR_TIMEOUT);
		timeout = true;
		this->state = I_ERROR;
		return true;
	}
	this->FlushWriteBuffer();
	return false;
}

bool InspSocket::Poll()
{
	int incoming = -1;
	bool n = true;

	switch (this->state)
	{
		case I_RESOLVING:
			log(DEBUG,"State = I_RESOLVING, calling DoResolve()");
			return this->DoResolve();
		break;
		case I_CONNECTING:
			log(DEBUG,"State = I_CONNECTING");
			this->SetState(I_CONNECTED);
			/* Our socket was in write-state, so delete it and re-add it
			 * in read-state.
			 */
			ServerInstance->SE->DelFd(this->fd);
			ServerInstance->SE->AddFd(this->fd,true,X_ESTAB_MODULE);
			return this->OnConnected();
		break;
		case I_LISTENING:
			length = sizeof (client);
			incoming = accept (this->fd, (sockaddr*)&client,&length);
			this->SetQueues(incoming);
			this->OnIncomingConnection(incoming,inet_ntoa(client.sin_addr));
			return true;
		break;
		case I_CONNECTED:
			n = this->OnDataReady();
			/* Flush any pending, but not till after theyre done with the event
			 * so there are less write calls involved. */
			this->FlushWriteBuffer();
			return n;
		break;
		default:
		break;
	}
	return true;
}

void InspSocket::SetState(InspSocketState s)
{
	log(DEBUG,"Socket state change");
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
void InspSocket::OnTimeout() { return; }
void InspSocket::OnClose() { return; }

InspSocket::~InspSocket()
{
	this->Close();
}

