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

#include <sstream>
#include <string>

enum InspSocketState { I_DISCONNECTED, I_CONNECTING, I_CONNECTED, I_LISTENING };

class InspSocket
{
private:
        int fd;
	std::string host;
	int port;
	InspSocketState state;
public:
	InspSocket();
	InspSocket(std::string host, int port, bool listening);
	void Poll();
	virtual int OnConnected();
	virtual int OnError();
	virtual int OnDisconnect();
	virtual int OnIncomingConnection();
	~InspSocket();
};
