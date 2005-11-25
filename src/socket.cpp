/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "inspircd_util.h"
#include "inspstring.h"
#include "helperfuncs.h"

extern FILE *log_file;
extern int boundPortCount;
extern int openSockfd[MAXSOCKS];
extern time_t TIME;
extern bool unlimitcore;
extern int MaxConn;

InspSocket::InspSocket()
{
	this->state = I_DISCONNECTED;
}

InspSocket::InspSocket(std::string host, int port, bool listening, unsigned long maxtime)
{
	if (listening) {
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
	        this->fd = -1;
	}
}

char* InspSocket::Read()
{
	int n = recv(this->fd,this->ibuf,sizeof(this->ibuf),0);
	if (n > 0)
	{
		return ibuf;
	}
	else
	{
		return NULL;
	}
}

// There are two possible outcomes to this function.
// It will either write all of the data, or an undefined amount.
// If an undefined amount is written the connection has failed
// and should be aborted.
int InspSocket::Write(std::string data)
{
	char* d = (char*)data.c_str();
	unsigned int written = 0;
	int n = 0;
	int s = data.length();
	while ((written < data.length()) && (n >= 0))
	{
		n = send(this->fd,d,s,0);
		if (n > 0)
		{
			// If we didnt write everything, advance
			// the pointers so that when we retry
			// the next time around the loop, we try
			// to write what we failed to write before.
			written += n;
			s -= n;
			d += n;
		}
	}
	return written;
}

bool InspSocket::Poll()
{
	if (time(NULL) > timeout_end)
	{
		this->OnTimeout();
		this->Close();
		this->OnError(I_ERR_TIMEOUT);
        	timeout = true;
		this->state = I_ERROR;
	        return false;
	}
        polls.fd = this->fd;
	state == I_CONNECTING ? polls.events = POLLOUT : polls.events = POLLIN;
	int ret = poll(&polls,1,1);

        if (ret > 0)
	{
		switch (this->state)
		{
			case I_CONNECTING:
				return this->OnConnected();
			break;
			case I_LISTENING:
				this->OnIncomingConnection();
			break;
			case I_CONNECTED:
				return this->OnDataReady();
			break;
			default:
			break;
		}
	}

	return true;
}

void InspSocket::SetState(InspSocketState s)
{
	this->state = s;
}

bool InspSocket::OnConnected() { return true; }
void InspSocket::OnError(InspSocketError e) { return; }
int InspSocket::OnDisconnect() { return 0; }
int InspSocket::OnIncomingConnection() { return 0; }
bool InspSocket::OnDataReady() { return true; }
void InspSocket::OnTimeout() { return; }
void InspSocket::OnClose() { return; }

InspSocket::~InspSocket()
{
	this->Close();
}

/*
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr)
int OpenTCPSocket (void)
*/
