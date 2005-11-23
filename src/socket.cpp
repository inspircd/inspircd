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
                                shutdown(this->fd,2);
                                close(this->fd);
                                this->fd = -1;
				this->state = I_ERROR;
                                return;
                        }
                }
                this->state = I_CONNECTING;
                return;
	}
}

void InspSocket::EngineTrigger()
{
	switch (this->state)
	{
		case I_CONNECTING:
			this->OnConnected();
		break;
		case I_LISTENING:
			this->OnIncomingConnection();
		break;
		case I_CONNECTED:
			this->OnDataReady();
		break;
		default:
		break;
	}
}

void InspSocket::SetState(InspSocketState s)
{
	this->state = s;
}

int InspSocket::OnConnected() { return 0; }
int InspSocket::OnError() { return 0; }
int InspSocket::OnDisconnect() { return 0; }
int InspSocket::OnIncomingConnection() { return 0; }
int InspSocket::OnDataReady() { return 0; }

InspSocket::~InspSocket()
{
}

/*
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr)
int OpenTCPSocket (void)
*/
