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

#ifndef __SOCKETENGINE_PORTS__
#define __SOCKETENGINE_PORTS__

#ifndef __sun
# error You need Solaris 10 or later to make use of this code.
#endif

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "globals.h"
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
	port_event_t events[MAX_DESCRIPTORS];
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

