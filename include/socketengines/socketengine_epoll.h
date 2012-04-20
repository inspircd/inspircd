/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SOCKETENGINE_EPOLL__
#define __SOCKETENGINE_EPOLL__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include "socketengine.h"
#include <sys/epoll.h>
#define EP_DELAY 5

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use linux 2.6 epoll().
 */
class EPollEngine : public SocketEngine
{
private:
	/** These are used by epoll() to hold socket events
	 */
	struct epoll_event* events;
public:
	/** Create a new EPollEngine
	 * @param Instance The creator of this object
	 */
	EPollEngine(InspIRCd* Instance);
	/** Delete an EPollEngine
	 */
	virtual ~EPollEngine();
	virtual bool AddFd(EventHandler* eh);
	virtual int GetMaxFds();
	virtual int GetRemainingFds();
	virtual bool DelFd(EventHandler* eh, bool force = false);
	virtual int DispatchEvents();
	virtual std::string GetName();
	virtual void WantWrite(EventHandler* eh);
};

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
public:
	/** Create a new instance of SocketEngine based on EpollEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new EPollEngine(Instance); }
};

#endif
