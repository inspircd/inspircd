/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef THREADENGINE
#define THREADENGINE

#ifdef WINDOWS
// #include "threadengines/threadengine_win32.h"
#else
#include "threadengines/threadengine_pthread.h"
#endif

class CoreExport Job : public classbase
{
 public:
	ModuleRef owner;
 private:
	bool cancelled;
 public:
	Job(Module* Creator) : owner(Creator), cancelled(false) {}
	virtual ~Job() {}
	/** Run in thread context.
	 */
	virtual void run() = 0;
	/** Run in the main thread context at some point after run() returns */
	virtual void finish() = 0;
	/** Override this to cause a thread wakeup, or simply poll for cancellation in run() */
	virtual void cancel() { cancelled = true; }
	/** Return true to ensure that this job finishes prior to the unload of the given module */
	virtual bool BlocksUnload(Module* m);
	inline bool IsCancelled() { return cancelled; }
};

// Mutexes are available. Be careful not to block the main thread.
// Condition variables (cross-thread signalling) are not yet available, because
// no use case for them has been presented

/** The background thread for config reading, so that reading from executable includes
 * does not block.
 */
class CoreExport ConfigReaderThread : public Job
{
	ServerConfig* Config;
 public:
	const std::string TheUserUID;
	ConfigReaderThread(const std::string &useruid)
		: Job(NULL), Config(new ServerConfig(REHASH_NEWCONF)), TheUserUID(useruid)
	{
	}

	virtual ~ConfigReaderThread()
	{
		delete Config;
	}

	void run();
	/** Run in the main thread to apply the configuration */
	void finish();
};

#endif

