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

#ifndef __INSP_SOCKET_H__
#define __INSP_SOCKET_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <string>

enum InspSocketState { I_DISCONNECTED, I_CONNECTING, I_CONNECTED, I_LISTENING, I_ERROR };
enum InspSocketError { I_ERR_TIMEOUT, I_ERR_SOCKET, I_ERR_CONNECT, I_ERR_BIND };

class InspSocket
{
private:
        int fd;
	std::string host;
	int port;
	InspSocketState state;
        sockaddr_in addr;
        in_addr addy;
        time_t timeout_end;
        bool timeout;
	pollfd polls;
	char ibuf[1024];
	sockaddr_in client;
	sockaddr_in server;
	socklen_t length;
public:
	InspSocket();
	InspSocket(int newfd);
	InspSocket(std::string host, int port, bool listening, unsigned long maxtime);
	virtual bool OnConnected();
	virtual void OnError(InspSocketError e);
	virtual int OnDisconnect();
	virtual bool OnDataReady();
	virtual void OnTimeout();
	virtual void OnClose();
	virtual char* Read();
	virtual int Write(std::string data);
	virtual int OnIncomingConnection(int newfd, char* ip);
	void SetState(InspSocketState s);
	bool Poll();
	virtual void Close();
	virtual ~InspSocket();
};

#endif
