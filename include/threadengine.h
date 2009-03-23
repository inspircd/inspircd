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
	virtual void SetExitFlag()
	{
		ExitFlag = true;
	}
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
	/** Notifies parent by making the SignalFD ready to read
	 * No requirements on locking
	 */
	void NotifyParent();
 public:
	SocketThread(InspIRCd* SI);
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

