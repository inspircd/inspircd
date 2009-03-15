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
