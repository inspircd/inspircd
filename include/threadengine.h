/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef THREADENGINE
#define THREADENGINE

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "base.h"

#ifdef WINDOWS
#include "threadengines/threadengine_win32.h"
#endif

class ThreadData;

/** Derive from this class to implement your own threaded sections of
 * code. Be sure to keep your code thread-safe and not prone to deadlocks
 * and race conditions if you MUST use threading!
 */
class CoreExport Thread
{
 private:
	/** Set to true when the thread is to exit
	 */
	bool ExitFlag;
 protected:
	/** Get thread's current exit status.
	 * (are we being asked to exit?)
	 */
	bool GetExitFlag()
	{
		return ExitFlag;
	}
 public:
	/** Opaque thread state managed by threading engine
	 */
	ThreadData* state;

	/** Set Creator to NULL at this point
	 */
	Thread() : ExitFlag(false), state(NULL)
	{
	}

	/* If the thread is running, you MUST join BEFORE deletion */
	virtual ~Thread();

	/** Override this method to put your actual
	 * threaded code here.
	 */
	virtual void Run() = 0;

	/** Signal the thread to exit gracefully.
	 */
	virtual void SetExitFlag();

	/** Join the thread (calls SetExitFlag and waits for exit)
	 */
	void join();
};


class CoreExport QueuedThread : public Thread
{
	ThreadQueueData queue;
 protected:
	/** Waits for an enqueue operation to complete
	 * You MUST hold the queue lock when you call this.
	 * It will be unlocked while you wait, and will be relocked
	 * before the function returns
	 */
	void WaitForQueue()
	{
		queue.Wait();
	}
 public:
	/** Lock queue.
	 */
	void LockQueue()
	{
		queue.Lock();
	}
	/** Unlock queue.
	 */
	void UnlockQueue()
	{
		queue.Unlock();
	}
	/** Unlock queue and wake up worker
	 */
	void UnlockQueueWakeup()
	{
		queue.Wakeup();
		queue.Unlock();
	}
	virtual void SetExitFlag()
	{
		queue.Lock();
		Thread::SetExitFlag();
		queue.Wakeup();
		queue.Unlock();
	}
};

class CoreExport SocketThread : public Thread
{
	ThreadQueueData queue;
	ThreadSignalData signal;
 protected:
	/** Waits for an enqueue operation to complete
	 * You MUST hold the queue lock when you call this.
	 * It will be unlocked while you wait, and will be relocked
	 * before the function returns
	 */
	void WaitForQueue()
	{
		queue.Wait();
	}
 public:
	/** Notifies parent by making the SignalFD ready to read
	 * No requirements on locking
	 */
	void NotifyParent();
	SocketThread();
	virtual ~SocketThread();
	/** Lock queue.
	 */
	void LockQueue()
	{
		queue.Lock();
	}
	/** Unlock queue.
	 */
	void UnlockQueue()
	{
		queue.Unlock();
	}
	/** Unlock queue and send wakeup to worker
	 */
	void UnlockQueueWakeup()
	{
		queue.Wakeup();
		queue.Unlock();
	}
	virtual void SetExitFlag()
	{
		queue.Lock();
		Thread::SetExitFlag();
		queue.Wakeup();
		queue.Unlock();
	}

	/**
	 * Called in the context of the parent thread after a notification
	 * has passed through the socket
	 */
	virtual void OnNotify() = 0;
};

#endif

