/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
*/

#ifndef __SOCKETENGINE_EPOLL__
#define __SOCKETENGINE_EPOLL__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#include "socketengine.h"
#include <sys/epoll.h>
#define EP_DELAY 5

class EPollEngine : public SocketEngine
{
private:
	struct epoll_event events[MAX_DESCRIPTORS];     /* Up to 64k sockets for epoll */
public:
	EPollEngine();
	virtual ~EPollEngine();
	virtual bool AddFd(int fd, bool readable, char type);
	virtual int GetMaxFds();
	virtual int GetRemainingFds();
	virtual bool DelFd(int fd);
	virtual int Wait(int* fdlist);
	virtual std::string GetName();
};

class SocketEngineFactory
{
public:
	SocketEngine* Create() { return new EPollEngine(); }
};

#endif
