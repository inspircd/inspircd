/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#ifndef THREADENGINE_H
#define THREADENGINE_H

#include "sockets.h"
#include "extensible.h"

class CoreExport Thread : public Pipe, public Extensible {
  private:
    /* Set to true to tell the thread to finish and we are waiting for it */
    bool exit;

  public:
    /* Handle for this thread */
    pthread_t handle;

    /** Threads constructor
     */
    Thread();

    /** Threads destructor
     */
    virtual ~Thread();

    /** Join to the thread, sets the exit state to true
     */
    void Join();

    /** Sets the exit state as true informing the thread we want it to shut down
     */
    void SetExitState();

    /** Exit the thread. Note that the thread still must be joined to free resources!
     */
    void Exit();

    /** Launch the thread
     */
    void Start();

    /** Returns the exit state of the thread
     * @return true if we want to exit
     */
    bool GetExitState() const;

    /** Called when this thread should be joined to
     */
    void OnNotify();

    /** Called when the thread is run.
     */
    virtual void Run() = 0;
};

class CoreExport Mutex {
  protected:
    /* A mutex, used to keep threads in sync */
    pthread_mutex_t mutex;

  public:
    /** Constructor
     */
    Mutex();

    /** Destructor
     */
    ~Mutex();

    /** Attempt to lock the mutex, will hang until a lock can be achieved
     */
    void Lock();

    /** Unlock the mutex, it must be locked first
     */
    void Unlock();

    /** Attempt to lock the mutex, will return true on success and false on fail
     * Does not block
     * @return true or false
     */
    bool TryLock();
};

class CoreExport Condition : public Mutex {
  private:
    /* A condition */
    pthread_cond_t cond;

  public:
    /** Constructor
     */
    Condition();

    /** Destructor
     */
    ~Condition();

    /** Called to wakeup the waiter
     */
    void Wakeup();

    /** Called to wait for a Wakeup() call
     */
    void Wait();
};

#endif // THREADENGINE_H
