/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
 * parts of the code which are NOT known threadsafe!
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

 public:

	/** Constructor
	 */
	ThreadEngine(InspIRCd* Instance);

	/** Destructor
	 */
	virtual ~ThreadEngine();

	/** Enable or disable system-wide mutex for threading.
	 * This MUST be called when you deal with ANYTHING that
	 * isnt known thread-safe, this INCLUDES STL!
	 * Remember that if you toggle the mutex you MUST UNSET
	 * IT LATER otherwise the program will DEADLOCK!
	 */
	virtual bool Mutex(bool enable) = 0;

	/** Run the newly created thread
	 */
	virtual void Run() = 0;

	/** Create a new thread. This takes an already allocated
	 * Thread* pointer and initializes it to use this threading
	 * engine. On failure, this function may throw a CoreException.
	 */
	virtual void Create(Thread* thread_to_init) = 0;

	/** This is called by the default destructor of the Thread
	 * class to ensure that the thread engine which created the thread
	 * is responsible for destroying it.
	 */
	virtual void FreeThread(Thread* thread) = 0;

	virtual const std::string GetName()
	{
		return "<pure-virtual>";
	}
};

/** Derive from this class to implement your own threaded sections of
 * code.
 */
class CoreExport Thread : public Extensible
{
 private:
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
	 * threaded code here
	 */
	virtual void Run() = 0;

	void SetExitFlag()
	{
		ExitFlag = true;
	}

	void ClearExitFlag()
	{
		ExitFlag = false;
	}

	bool GetExitFlag()
	{
		return ExitFlag;
	}
};



#endif

