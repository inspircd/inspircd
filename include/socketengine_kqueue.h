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

#ifndef __SOCKETENGINE_KQUEUE__
#define __SOCKETENGINE_KQUEUE__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine.h"

class KQueueEngine : public SocketEngine
{
private:
	struct kevent ke_list[MAX_DESCRIPTORS];	/* Up to 64k sockets for kqueue */
	struct timespec ts;			/* kqueue delay value */
public:
	KQueueEngine();
	virtual ~KQueueEngine();
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
	SocketEngine* Create() { return new KQueueEngine(); }
};

#endif
