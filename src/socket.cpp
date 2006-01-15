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
#include "socket.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "socketengine.h"


extern InspIRCd* ServerInstance;
extern time_t TIME;

InspSocket* socket_ref[MAX_DESCRIPTORS];

InspSocket::InspSocket()
{
	this->state = I_DISCONNECTED;
}

InspSocket::InspSocket(int newfd, char* ip)
{
	this->fd = newfd;
	this->state = I_CONNECTED;
	this->IP = ip;
	ServerInstance->SE->AddFd(this->fd,true,X_ESTAB_MODULE);
	socket_ref[this->fd] = this;
}

InspSocket::InspSocket(std::string host, int port, bool listening, unsigned long maxtime)
{
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
			if (BindSocket(this->fd,this->client,this->server,port,(char*)host.c_str()) == ERROR)
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
	} else {
		char* ip;
		this->host = host;
		hostent* hoste = gethostbyname(host.c_str());
		if (!hoste) {
			ip = (char*)host.c_str();
		} else {
			struct in_addr* ia = (in_addr*)hoste->h_addr;
			ip = inet_ntoa(*ia);
		}

		this->IP = ip;

                timeout_end = time(NULL)+maxtime;
                timeout = false;
                if ((this->fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		{
			this->state = I_ERROR;
			this->OnError(I_ERR_SOCKET);
                        return;
		}
		this->port = port;
                inet_aton(ip,&addy);
                addr.sin_family = AF_INET;
                addr.sin_addr = addy;
                addr.sin_port = htons(this->port);

                int flags;
                flags = fcntl(this->fd, F_GETFL, 0);
                fcntl(this->fd, F_SETFL, flags | O_NONBLOCK);

                if(connect(this->fd, (sockaddr*)&this->addr,sizeof(this->addr)) == -1)
                {
                        if (errno != EINPROGRESS)
                        {
				this->Close();
				this->OnError(I_ERR_CONNECT);
				this->state = I_ERROR;
                                return;
                        }
                }
                this->state = I_CONNECTING;
		ServerInstance->SE->AddFd(this->fd,false,X_ESTAB_MODULE);
		socket_ref[this->fd] = this;
                return;
	}
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
	int n = recv(this->fd,this->ibuf,sizeof(this->ibuf),0);
	if (n > 0)
	{
		ibuf[n] = 0;
		return ibuf;
	}
	else
	{
		if (n == EAGAIN)
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
	this->Buffer = this->Buffer + data;
	this->FlushWriteBuffer();
	return data.length();
}

void InspSocket::FlushWriteBuffer()
{
	int result = 0;
	if (this->Buffer.length())
	{
		result = send(this->fd,this->Buffer.c_str(),this->Buffer.length(),0);
		if (result > 0)
		{
			/* If we wrote some, advance the buffer forwards */
			char* n = (char*)this->Buffer.c_str();
			n += result;
			this->Buffer = n;
		}
	}
}

bool InspSocket::Timeout(time_t current)
{
	if ((this->state == I_CONNECTING) && (current > timeout_end))
	{
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
	if (this->Buffer.length())
		this->FlushWriteBuffer();
	return false;
}

bool InspSocket::Poll()
{
	int incoming = -1;
	
	switch (this->state)
	{
		case I_CONNECTING:
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
			this->OnIncomingConnection(incoming,inet_ntoa(client.sin_addr));
			return true;
		break;
		case I_CONNECTED:
			return this->OnDataReady();
		break;
		default:
		break;
	}

	if (this->Buffer.length())
		this->FlushWriteBuffer();

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

