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


#ifndef __SOCKETENGINE_KQUEUE__
#define __SOCKETENGINE_KQUEUE__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine.h"

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use FreeBSD kqueue().
 */
class KQueueEngine : public SocketEngine
{
private:
	/** These are used by kqueue() to hold socket events
	 */
	struct kevent* ke_list;
	/** This is a specialised time value used by kqueue()
	 */
	struct timespec ts;
public:
	/** Create a new KQueueEngine
	 * @param Instance The creator of this object
	 */
	KQueueEngine(InspIRCd* Instance);
	/** Delete a KQueueEngine
	 */
	virtual ~KQueueEngine();
	virtual bool AddFd(EventHandler* eh);
	virtual int GetMaxFds();
	virtual int GetRemainingFds();
	virtual bool DelFd(EventHandler* eh, bool force = false);
	virtual int DispatchEvents();
	virtual std::string GetName();
	virtual void WantWrite(EventHandler* eh);
	virtual void RecoverFromFork();
};

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
 public:
	/** Create a new instance of SocketEngine based on KQueueEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new KQueueEngine(Instance); }
};

#endif
