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

#ifndef __SOCKETENGINE_EPOLL__
#define __SOCKETENGINE_EPOLL__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "globals.h"
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
	struct epoll_event events[MAX_DESCRIPTORS];
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
	virtual bool DelFd(EventHandler* eh);
	virtual int Wait(EventHandler** fdlist);
	virtual std::string GetName();
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
