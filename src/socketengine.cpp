/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "socketengine.h"

int EventHandler::GetFd()
{
	return this->fd;
}

void EventHandler::SetFd(int FD)
{
	this->fd = FD;
}

bool EventHandler::Readable()
{
	return true;
}

bool EventHandler::Writeable()
{
	return false;
}

void SocketEngine::WantWrite(EventHandler* eh)
{
}

SocketEngine::SocketEngine(InspIRCd* Instance) : ServerInstance(Instance)
{
	memset(ref, 0, sizeof(ref));
}

SocketEngine::~SocketEngine()
{
}

bool SocketEngine::AddFd(EventHandler* eh)
{
	return true;
}

bool SocketEngine::HasFd(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return false;
	return ref[fd];
}

EventHandler* SocketEngine::GetRef(int fd)
{
	if ((fd < 0) || (fd > MAX_DESCRIPTORS))
		return 0;
	return ref[fd];
}

bool SocketEngine::DelFd(EventHandler* eh, bool force)
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

int SocketEngine::DispatchEvents()
{
	return 0;
}

std::string SocketEngine::GetName()
{
	return "misconfigured";
}

