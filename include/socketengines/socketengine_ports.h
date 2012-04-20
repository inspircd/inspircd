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


#ifndef __SOCKETENGINE_PORTS__
#define __SOCKETENGINE_PORTS__

#ifndef __sun
# error You need Solaris 10 or later to make use of this code.
#endif

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include "socketengine.h"
#include <port.h>

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use solaris 10 I/O completion ports
 */
class PortsEngine : public SocketEngine
{
private:
	/** These are used by epoll() to hold socket events
	 */
	port_event_t* events;
public:
	/** Create a new PortsEngine
	 * @param Instance The creator of this object
	 */
	PortsEngine(InspIRCd* Instance);
	/** Delete a PortsEngine
	 */
	virtual ~PortsEngine();
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
	/** Create a new instance of SocketEngine based on PortsEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new PortsEngine(Instance); }
};

#endif

