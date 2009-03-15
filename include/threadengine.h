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
 protected:

	 /** Creator instance
	  */
	 InspIRCd* ServerInstance;

	 /** New Thread being created.
	  */
	 Thread* NewThread;

	 /** Enable or disable system-wide mutex for threading.
	  * Remember that if you toggle the mutex you MUST UNSET
	  * IT LATER otherwise the program will DEADLOCK!
	  * It is recommended that you AVOID USE OF THIS METHOD
	  * and use your own Mutex class, this function is mainly
	  * reserved for use by the core and by the Thread engine
	  * itself.
	  * @param enable True to lock the mutex.
	  */
	 virtual bool Mutex(bool enable) = 0;
 public:

	/** Constructor.
	 * @param Instance Creator object
	 */
	ThreadEngine(InspIRCd* Instance);

	/** Destructor
	 */
	virtual ~ThreadEngine();

	/** Lock the system wide mutex. See the documentation for
	 * ThreadEngine::Mutex().
	 */
	void Lock() { this->Mutex(true); }

	/** Unlock the system wide mutex. See the documentation for
	 * ThreadEngine::Mutex()
	 */
	void Unlock() { this->Mutex(false); }

	/** Run the newly created thread.
	 */
	virtual void Run() = 0;

	/** Create a new thread. This takes an already allocated
	 * Thread* pointer and initializes it to use this threading
	 * engine. On failure, this function may throw a CoreException.
	 * @param thread_to_init Pointer to a newly allocated Thread
	 * derived object.
	 */
	virtual void Create(Thread* thread_to_init) = 0;

	/** This is called by the default destructor of the Thread
	 * class to ensure that the thread engine which created the thread
	 * is responsible for destroying it.
	 * @param thread Existing and active thread to delete.
	 */
	virtual void FreeThread(Thread* thread) = 0;

	/** Returns the thread engine's name for display purposes
	 * @return The thread engine name
	 */
	virtual const std::string GetName()
	{
		return "<pure-virtual>";
	}
};

/** The Mutex class represents a mutex, which can be used to keep threads
 * properly synchronised. Use mutexes sparingly, as they are a good source
 * of thread deadlocks etc, and should be avoided except where absolutely
 * neccessary. Note that the internal behaviour of the mutex varies from OS
 * to OS depending on the thread engine, for example in windows a Mutex
 * in InspIRCd uses critical sections, as they are faster and simpler to
 * manage.
 */
class CoreExport Mutex : public Extensible
{
 protected:

	/** Creator object
	 */
	InspIRCd* ServerInstance;

	/** Enable or disable the Mutex. This method has somewhat confusing
	 * wording (e.g. the function name and parameters) so it is protected
	 * in preference of the Lock() and Unlock() methods which are user-
	 * accessible.
	 *
	 * @param enable True to enable the mutex (enter it) and false to
	 * disable the mutex (leave it).
	 */
	virtual void Enable(bool enable) = 0;
 public:

	/** Constructor.
	 * @param Instance Creator object
	 */
	Mutex(InspIRCd* Instance);

	/** Enter/enable the mutex lock.
	 */
	void Lock() { Enable(true); }

	/** Leave/disable the mutex lock.
	 */
	void Unlock() { Enable(false); }

	/** Destructor
	 */
	~Mutex() { }
};

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

	/** Creator thread engine
	 */
	ThreadEngine* Creator;

	/** Set Creator to NULL at this point
	 */
	Thread() : ExitFlag(false), Creator(NULL)
	{
	}

	/** If this thread has a Creator set, call it to
	 * free the thread
	 */
	virtual ~Thread()
	{
		if (Creator)
			Creator->FreeThread(this);
	}

	/** Override this method to put your actual
	 * threaded code here.
	 */
	virtual void Run() = 0;

	/** Signal the thread to exit gracefully.
	 */
	void SetExitFlag()
	{
		ExitFlag = true;
	}

	/** Cancel an exit state.
	 */
	void ClearExitFlag()
	{
		ExitFlag = false;
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

