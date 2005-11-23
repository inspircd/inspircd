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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <string>

enum InspSocketState { I_DISCONNECTED, I_CONNECTING, I_CONNECTED, I_LISTENING, I_ERROR };

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
public:
	InspSocket();
	InspSocket(std::string host, int port, bool listening, unsigned long maxtime);
	virtual int OnConnected();
	virtual int OnError();
	virtual int OnDisconnect();
	virtual int OnDataReady();
	virtual int OnIncomingConnection();
	void SetState(InspSocketState s);
	void EngineTrigger();
	virtual ~InspSocket();
};
