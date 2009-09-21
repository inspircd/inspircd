/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
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
#include "inspircd.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "socketengine.h"

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
	 */
	KQueueEngine();
	/** Delete a KQueueEngine
	 */
	virtual ~KQueueEngine();
	virtual bool AddFd(EventHandler* eh, bool writeFirst = false);
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
	SocketEngine* Create() { return new KQueueEngine; }
};

#endif
