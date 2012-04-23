/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


#ifndef __SOCKETENGINE_POLL__
#define __SOCKETENGINE_POLL__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "inspircd.h"
#include "socketengine.h"

#ifndef WINDOWS
	#ifndef __USE_XOPEN
    	    #define __USE_XOPEN /* fuck every fucking OS ever made. needed by poll.h to work.*/
	#endif
	#include <poll.h>
	#include <sys/poll.h>
#else
	/* *grumble* */
	#define struct pollfd WSAPOLLFD
	#define poll WSAPoll
#endif

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use poll().
 */
class PollEngine : public SocketEngine
{
private:
	/** These are used by poll() to hold socket events
	 */
	struct pollfd *events;
	/** This map maps fds to an index in the events array.
	 */
	std::map<int, unsigned int> fd_mappings;
public:
	/** Create a new PollEngine
	 * @param Instance The creator of this object
	 */
	PollEngine(InspIRCd* Instance);
	/** Delete a PollEngine
	 */
	virtual ~PollEngine();
	virtual bool AddFd(EventHandler* eh);
	virtual EventHandler* GetRef(int fd);
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
	/** Create a new instance of SocketEngine based on PollEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new PollEngine(Instance); }
};

#endif
