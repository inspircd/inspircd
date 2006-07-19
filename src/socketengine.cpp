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

#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif
#ifdef USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#include <vector>
#include <string>
#include "socketengine.h"
#include "helperfuncs.h"

SocketEngine::SocketEngine()
{
}

SocketEngine::~SocketEngine()
{
	log(DEBUG,"SocketEngine::~SocketEngine()");
}

char SocketEngine::GetType(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return X_EMPTY_SLOT;
	/* Mask off the top bit used for 'read/write' state */
	return (ref[fd] & ~0x80);
}

bool SocketEngine::AddFd(int fd, bool readable, char type)
{
	return true;
}

bool SocketEngine::HasFd(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;
	return (ref[fd] != 0);
}

bool SocketEngine::DelFd(int fd)
{
	return true;
}

int SocketEngine::GetMaxFds()
{
	return 0;
}

int SocketEngine::GetRemainingFds()
{
	return 0;
}

int SocketEngine::Wait(int* fdlist)
{
	return 0;
}

std::string SocketEngine::GetName()
{
	return "misconfigured";
}
