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

#ifndef __SOCKETENGINE_SELECT__
#define __SOCKETENGINE_SELECT__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "globals.h"
#include "inspircd.h"
#include "socketengine.h"

class SelectEngine : public SocketEngine
{
private:
	std::map<int,int> fds;		/* List of file descriptors being monitored */
	fd_set wfdset, rfdset;		/* Readable and writeable sets for select() */
public:
	SelectEngine();
	virtual ~SelectEngine();
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
	SocketEngine* Create() { return new SelectEngine(); }
};

#endif
