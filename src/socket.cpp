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
#include "message.h"


extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern time_t TIME;

InspSocket* socket_ref[MAX_DESCRIPTORS];

InspSocket::InspSocket()
{
	this->state = I_DISCONNECTED;
	this->fd = -1;
	this->ClosePending = false;
}

InspSocket::InspSocket(int newfd, char* ip)
{
	this->fd = newfd;
	this->state = I_CONNECTED;
	strlcpy(this->IP,ip,MAXBUF);
	this->ClosePending = false;
	ServerInstance->SE->AddFd(this->fd,true,X_ESTAB_MODULE);
	socket_ref[this->fd] = this;
}

InspSocket::InspSocket(const std::string &ahost, int aport, bool listening, unsigned long maxtime) : fd(-1)
{
	strlcpy(host,ahost.c_str(),MAXBUF);
	this->ClosePending = false;
	if (listening) {
		if ((this->fd = OpenTCPSocket()) == ERROR)
		{
			this->fd = -1;
			this->ClosePending = true;
			this->state = I_ERROR;
			this->OnError(I_ERR_SOCKET);
			log(DEBUG,"OpenTCPSocket() error");
                        return;
		}
		else
		{
			if (!BindSocket(this->fd,this->client,this->server,aport,(char*)ahost.c_str()))
			{
				this->Close();
				this->fd = -1;
				this->ClosePending = true;
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
		strlcpy(this->host,ahost.c_str(),MAXBUF);
		this->port = aport;

		if (!inet_aton(host,&addy))
		{
			log(DEBUG,"Attempting to resolve %s",this->host);
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
			log(DEBUG,"No need to resolve %s",this->host);
			strlcpy(this->IP,host,MAXBUF);
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
		std::string res_ip = dns.GetResultIP();
		if (res_ip != "")
		{
			strlcpy(this->IP,res_ip.c_str(),MAXBUF);
			socket_ref[this->fd] = NULL;
		}
		else
		{
			log(DEBUG,"Socket DNS failure");
			this->Close();
			this->state = I_ERROR;
			this->OnError(I_ERR_RESOLVE);
			this->ClosePending = true;
			this->fd = -1;
			return false;
		}
		return this->DoConnect();
	}
	log(DEBUG,"No result for socket yet!");
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
bool InspSocket::BindAddr()
{
	in_addr n;
	ConfigReader Conf;

	log(DEBUG,"In InspSocket::BindAddr()");
	for (int j =0; j < Conf.Enumerate("bind"); j++)
	{
		std::string Type = Conf.ReadValue("bind","type",j);
		std::string IP = Conf.ReadValue("bind","address",j);
		if (Type == "servers")
		{
			if ((IP != "*") && (IP != "127.0.0.1") && (IP != ""))
			{
				sockaddr_in s;
				char resolved_addr[MAXBUF];

				if (!inet_aton(IP.c_str(),&n))
				{
					/* If they gave a hostname, bind to the IP it resolves to */
					log(DEBUG,"Resolving host %s",IP.c_str());
					if (CleanAndResolve(resolved_addr, IP.c_str(), true))
			                {
						log(DEBUG,"Resolved host %s to %s",IP.c_str(),resolved_addr);
			                        IP = resolved_addr;
					}
				}
		
				if (inet_aton(IP.c_str(),&n))
				{
					log(DEBUG,"Found an IP to bind to: %s",IP.c_str());
					s.sin_addr = n;
					s.sin_family = AF_INET;
					if (bind(this->fd,(struct sockaddr*)&s,sizeof(s)) < 0)
					{
						log(DEBUG,"Cant bind()");
						this->state = I_ERROR;
						this->OnError(I_ERR_BIND);
						this->fd = -1;
						return false;
					}
					log(DEBUG,"bind() reports outbound fd bound to ip %s",IP.c_str());
					return true;
				}
				else
				{
					log(DEBUG,"Address '%s' was not an IP address",IP.c_str());
				}
			}
		}
	}
	log(DEBUG,"Found no suitable IPs to bind, binding INADDR_ANY");
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
		this->ClosePending = true;
		this->fd = -1;
		return false;
	}

	if (!this->BindAddr())
		return false;

	log(DEBUG,"Part 2 DoConnect() %s",this->IP);
	inet_aton(this->IP,&addy);
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
			this->Close();
			this->state = I_ERROR;
			this->fd = -1;
			this->ClosePending = true;
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
		this->ClosePending = true;
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
			log(DEBUG,"EOF or error on socket: %s",strerror(errno));
			return NULL;
		}
	}
}

void InspSocket::MarkAsClosed()
{
	log(DEBUG,"Marked as closed");
	this->ClosePending = true;
}

// There are two possible outcomes to this function.
// It will either write all of the data, or an undefined amount.
// If an undefined amount is written the connection has failed
// and should be aborted.
int InspSocket::Write(const std::string &data)
{
	if (this->ClosePending)
		return false;

	/*int result = write(this->fd,data.c_str(),data.length());
	if (result < 1)
		return false;
	return true;*/

	/* Try and append the data to the back of the queue, and send it on its way
	 */
	outbuffer.push_back(data);
	return (!this->FlushWriteBuffer());
}

bool InspSocket::FlushWriteBuffer()
{
	if (this->ClosePending)
		return true;

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
				log(DEBUG,"Write error on socket: %s",strerror(errno));
				this->OnError(I_ERR_WRITE);
				this->state = I_ERROR;
				this->ClosePending = true;
				return true;
			}
		}
	}
	return (fd < 0);
}

bool InspSocket::Timeout(time_t current)
{
	if (!socket_ref[this->fd] || !ServerInstance->SE->HasFd(this->fd))
	{
		log(DEBUG,"No FD or socket ref");
		return false;
	}

	if (this->ClosePending)
	{
		log(DEBUG,"Close is pending");
		return true;
	}

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
		this->ClosePending = true;
		return true;
	}
	return this->FlushWriteBuffer();
}

bool InspSocket::Poll()
{
	if (!socket_ref[this->fd] || !ServerInstance->SE->HasFd(this->fd))
		return false;

	int incoming = -1;
	bool n = true;

	if ((fd < 0) || (fd > MAX_DESCRIPTORS) || (this->ClosePending))
		return false;

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
