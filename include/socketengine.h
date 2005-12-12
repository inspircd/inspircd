/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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

#ifndef __SOCKETENGINE__
#define __SOCKETENGINE__

#include <vector>
#include <string>
#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#ifdef USE_EPOLL
#include <sys/epoll.h>
#define EP_DELAY 5
#endif
#ifdef USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

const char X_EMPTY_SLOT		= 0;
const char X_LISTEN             = 1;
const char X_ESTAB_CLIENT       = 2;
const char X_ESTAB_MODULE       = 3;
const char X_ESTAB_DNS          = 4;

const char X_READBIT            = 0x80;

class SocketEngine {

	std::vector<int> fds;
	int EngineHandle;
#ifdef USE_SELECT
	fd_set wfdset, rfdset;
#endif
#ifdef USE_KQUEUE
	struct kevent ke_list[65535];
	struct timespec ts;
#endif
#ifdef USE_EPOLL
	struct epoll_event events[65535];
#endif

public:

	SocketEngine();
	~SocketEngine();
	bool AddFd(int fd, bool readable, char type);
	char GetType(int fd);
	bool DelFd(int fd);
	bool Wait(std::vector<int> &fdlist);
	std::string GetName();
};

#endif
