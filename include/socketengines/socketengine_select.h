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


#ifndef __SOCKETENGINE_SELECT__
#define __SOCKETENGINE_SELECT__

#include <vector>
#include <string>
#include <map>
#ifndef WINDOWS
#include <sys/select.h>
#endif // WINDOWS
#include "inspircd_config.h"
#include "inspircd.h"
#include "socketengine.h"

class InspIRCd;

/** A specialisation of the SocketEngine class, designed to use traditional select().
 */
class SelectEngine : public SocketEngine
{
private:
	/** Because select() does not track an fd list for us between calls, we have one of our own
	 */
	std::set<int> fds;
	/** List of writeable ones (WantWrite())
	 */
	std::vector<bool> writeable;
	/** The read set and write set, populated before each call to select().
	 */
	fd_set wfdset, rfdset, errfdset;

public:
	/** Create a new SelectEngine
	 * @param Instance The creator of this object
	 */
	SelectEngine(InspIRCd* Instance);
	/** Delete a SelectEngine
	 */
	virtual ~SelectEngine();
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
	/** Create a new instance of SocketEngine based on SelectEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new SelectEngine(Instance); }
};

#endif
