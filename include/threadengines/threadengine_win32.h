/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __THREADENGINE_WIN32THREAD__
#define __THREADENGINE_WIN32THREAD__

#include "inspircd_config.h"
#include "base.h"

class InspIRCd;
class Thread;

/** The ThreadEngine class has the responsibility of initialising
 * Thread derived classes. It does this by creating operating system
 * level threads which are then associated with the class transparently.
 * This allows Thread classes to be derived without needing to know how
 * the OS implements threads. You should ensure that any sections of code
 * that use threads are threadsafe and do not interact with any other
 * parts of the code which are NOT known threadsafe! If you really MUST
 * access non-threadsafe code from a Thread, use the Mutex class to wrap
 * access to the code carefully.
 */
class CoreExport ThreadEngine : public Extensible
{
 public:

	ThreadEngine(InspIRCd* Instance);

	virtual ~ThreadEngine();

	static DWORD WINAPI Entry(void* parameter);

	/** Create a new thread. This takes an already allocated
	  * Thread* pointer and initializes it to use this threading
	  * engine. On failure, this function may throw a CoreException.
	  * @param thread_to_init Pointer to a newly allocated Thread
	  * derived object.
	  */
	void Start(Thread* thread_to_init);

	/** Returns the thread engine's name for display purposes
	 * @return The thread engine name
	 */
	const std::string GetName()
	{
		return "windows-thread";
	}
};

class CoreExport ThreadData
{
 public:
	HANDLE handle;
	void FreeThread(Thread* toFree);
};

/** The Mutex class represents a mutex, which can be used to keep threads
 * properly synchronised. Use mutexes sparingly, as they are a good source
 * of thread deadlocks etc, and should be avoided except where absolutely
 * neccessary. Note that the internal behaviour of the mutex varies from OS
 * to OS depending on the thread engine, for example in windows a Mutex
 * in InspIRCd uses critical sections, as they are faster and simpler to
 * manage.
 */
class CoreExport Mutex
{
 private:
	CRITICAL_SECTION wutex;
 public:
	Mutex()
	{
		InitializeCriticalSection(&wutex);
	}
	void Lock()
	{
		EnterCriticalSection(&wutex);
	}
	void Unlock()
	{
		LeaveCriticalSection(&wutex);
	}
	~Mutex()
	{
		DeleteCriticalSection(&wutex);
	}
};

class ThreadQueueData
{
	CRITICAL_SECTION mutex;
	HANDLE event;
 public:
	ThreadQueueData()
	{
		InitializeCriticalSection(&mutex);
		event = CreateEvent(NULL, false, false, NULL);
	}

	~ThreadQueueData()
	{
		DeleteCriticalSection(&mutex);
	}

	void Lock()
	{
		EnterCriticalSection(&mutex);
	}

	void Unlock()
	{
		LeaveCriticalSection(&mutex);
	}

	void Wakeup()
	{
		PulseEvent(event);
	}

	void Wait()
	{
		LeaveCriticalSection(&mutex);
		WaitForSingleObject(event, INFINITE);
		EnterCriticalSection(&mutex);
	}
};

class ThreadSignalData
{
 public:
	int connFD;
	ThreadSignalData()
	{
		connFD = -1;
	}
};

#endif

