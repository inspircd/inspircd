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

#ifndef __THREADENGINE__
#define __THREADENGINE__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "base.h"

/** Derive from this class to implement your own threaded sections of
 * code. Be sure to keep your code thread-safe and not prone to deadlocks
 * and race conditions if you MUST use threading!
 */
class CoreExport Thread : public Extensible
{
 private:
	/** Set to true when the thread is to exit
	 */
	bool ExitFlag;
 public:
	/** Opaque thread state managed by threading engine
	 */
	ThreadData* state;

	/** Set Creator to NULL at this point
	 */
	Thread() : ExitFlag(false), state(NULL)
	{
	}

	/** If this thread has a Creator set, call it to
	 * free the thread
	 */
	virtual ~Thread()
	{
		if (state)
		{
			state->FreeThread(this);
			delete state;
		}
	}

	/** Override this method to put your actual
	 * threaded code here.
	 */
	virtual void Run() = 0;

	/** Signal the thread to exit gracefully.
	 */
	void SetExitFlag(bool value)
	{
		ExitFlag = value;
	}

	/** Get thread's current exit status.
	 * (are we being asked to exit?)
	 */
	bool GetExitFlag()
	{
		return ExitFlag;
	}
};

#endif

