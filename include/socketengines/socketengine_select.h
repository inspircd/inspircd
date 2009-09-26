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

/** A specialisation of the SocketEngine class, designed to use traditional select().
 */
class SelectEngine : public SocketEngine
{
public:
	/** Create a new SelectEngine
	 */
	SelectEngine();
	/** Delete a SelectEngine
	 */
	virtual ~SelectEngine();
	virtual bool AddFd(EventHandler* eh, int event_mask);
	virtual bool DelFd(EventHandler* eh, bool force = false);
	void OnSetEvent(EventHandler* eh, int, int);
	virtual int DispatchEvents();
	virtual std::string GetName();
};

/** Creates a SocketEngine
 */
class SocketEngineFactory
{
public:
	/** Create a new instance of SocketEngine based on SelectEngine
	 */
	SocketEngine* Create() { return new SelectEngine; }
};

#endif
