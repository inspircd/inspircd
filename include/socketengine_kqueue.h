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

#ifndef __SOCKETENGINE_KQUEUE__
#define __SOCKETENGINE_KQUEUE__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "globals.h"
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
	struct kevent ke_list[MAX_DESCRIPTORS];
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
	virtual bool DelFd(EventHandler* eh);
	virtual int Wait(EventHandler** fdlist);
	virtual std::string GetName();
};

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
 public:
	/** Create a new instance of SocketEngine based on KQueueEngine
	 */
	SocketEngine* Create(InspIRCd* Instance) { return new KQueueEngine(InspIRCd* Instance); }
};

#endif
