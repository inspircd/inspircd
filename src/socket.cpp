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
#include <string>
#include <unistd.h>
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

InspSocket::InspSocket(std::string host, int port, bool listening)
{
}

void InspSocket::Poll()
{
}

int InspSocket::OnConnected() { }
int InspSocket::OnError() { }
int InspSocket::OnDisconnect() { }
int InspSocket::OnIncomingConnection() { }

InspSocket::~InspSocket()
{
}

/*
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr)
int OpenTCPSocket (void)
*/
