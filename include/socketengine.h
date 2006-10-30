/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
*/

#ifndef __SOCKETENGINE__
#define __SOCKETENGINE__

#include <vector>
#include <string>
#include <map>
#include "inspircd_config.h"
#include "base.h"

/** Types of event an EventHandler may receive.
 * EVENT_READ is a readable file descriptor,
 * and EVENT_WRITE is a writeable file descriptor.
 */
enum EventType
{
	EVENT_READ	=	0,
	EVENT_WRITE	=	1
};

class InspIRCd;

/** This class is a basic I/O handler class.
 * Any object which wishes to receive basic I/O events
 * from the socketengine must derive from this class and
 * implement the HandleEvent() method. The derived class
 * must then be added to SocketEngine using the method
 * SocketEngine::AddFd(), after which point the derived
 * class will receive events to its HandleEvent() method.
 * The derived class should also implement one of Readable()
 * and Writeable(). In the current implementation, only
 * Readable() is used. If this returns true, the socketengine
 * inserts a readable socket. If it is false, the socketengine
 * inserts a writeable socket. The derived class should never
 * change the value this function returns without first
 * deleting the socket from the socket engine. The only
 * requirement beyond this for an event handler is that it
 * must have a file descriptor. What this file descriptor
 * is actually attached to is completely up to you.
 */
class EventHandler : public Extensible
{
 protected:
	/** File descriptor.
	 * All events which can be handled
	 * must have a file descriptor.
	 * This allows you to add events for
	 * sockets, fifo's, pipes, and various
	 * other forms of IPC.
	 */
	int fd;
 public:
	/** Get the current file descriptor
	 * @return The file descriptor of this handler
	 */
	int GetFd();

	/** Set a new file desciptor
	 * @param FD The new file descriptor. Do not
	 * call this method without first deleting the
	 * object from the SocketEngine if you have
	 * added it to a SocketEngine instance.
	 */
	void SetFd(int FD);

	/** Constructor
	 */
	EventHandler() {}

	/** Destructor
	 */
	virtual ~EventHandler() {}

	/** Override this function to indicate readability.
	 * @return This should return true if the function
	 * wishes to receive EVENT_READ events. Do not change
	 * what this function returns while the event handler
	 * is still added to a SocketEngine instance!
	 * If this function is unimplemented, the base class
	 * will return true.
	 */
	virtual bool Readable();

	/** Override this function to indicate writeability.
	 * @return This should return true if the function
	 * wishes to receive EVENT_WRITE events. Do not change
	 * what this function returns while the event handler
	 * is still added to a SocketEngine instance!
	 * If this function is unimplemented, the base class
	 * will return false.
	 */
	virtual bool Writeable();

	/** Process an I/O event.
	 * You MUST implement this function in your derived
	 * class, and it will be called whenever read or write
	 * events are received, depending on what your functions
	 * Readable() and Writeable() returns.
	 * @param et either one of EVENT_READ for read events,
	 * and EVENT_WRITE for write events.
	 */
	virtual void HandleEvent(EventType et) = 0;
};

/** Provides basic file-descriptor-based I/O support.
 * The actual socketengine class presents the
 * same interface on all operating systems, but
 * its private members and internal behaviour
 * should be treated as blackboxed, and vary
 * from system to system and upon the config
 * settings chosen by the server admin. The current
 * version supports select, epoll and kqueue.
 * The configure script will enable a socket engine
 * based upon what OS is detected, and will derive
 * a class from SocketEngine based upon what it finds.
 * The derived classes file will also implement a
 * classfactory, SocketEngineFactory, which will
 * create a derived instance of SocketEngine using
 * polymorphism so that the core and modules do not
 * have to be aware of which SocketEngine derived
 * class they are using.
 */
class SocketEngine : public Extensible
{
protected:
	/** Owner/Creator
	 */
	InspIRCd* ServerInstance;
	/** Handle to socket engine, where needed.
	 */
	int EngineHandle;
	/** Current number of descriptors in the engine
	 */
	int CurrentSetSize;
	/** Reference table, contains all current handlers
	 */
	EventHandler* ref[MAX_DESCRIPTORS];
public:

	/** Constructor.
	 * The constructor transparently initializes
	 * the socket engine which the ircd is using.
	 * Please note that if there is a catastrophic
	 * failure (for example, you try and enable
	 * epoll on a 2.4 linux kernel) then this
	 * function may bail back to the shell.
	 * @param Instance The creator/owner of this object
	 */
	SocketEngine(InspIRCd* Instance);

	/** Destructor.
	 * The destructor transparently tidies up
	 * any resources used by the socket engine.
	 */
	virtual ~SocketEngine();

	/** Add an EventHandler object to the engine.
	 * Use AddFd to add a file descriptor to the
	 * engine and have the socket engine monitor
	 * it. You must provide an object derived from
	 * EventHandler which implements HandleEvent()
	 * and optionally Readable() and Writeable().
	 * @param eh An event handling object to add
	 */
	virtual bool AddFd(EventHandler* eh);

	virtual void WantWrite(EventHandler* eh);

	/** Returns the maximum number of file descriptors
	 * you may store in the socket engine at any one time.
	 * @return The maximum fd value
	 */
	virtual int GetMaxFds();

	/** Returns the number of file descriptor slots
	 * which are available for storing fds.
	 * @return The number of remaining fd's
	 */
	virtual int GetRemainingFds();

	/** Delete an event handler from the engine.
	 * This function call deletes an EventHandler
	 * from the engine, returning true if it succeeded
	 * and false if it failed. This does not free the
	 * EventHandler pointer using delete, if this is
	 * required you must do this yourself.
	 * @param eh The event handler object to remove
	 * @return True if the event handler was removed
	 */
	virtual bool DelFd(EventHandler* eh);

	/** Returns true if a file descriptor exists in
	 * the socket engine's list.
	 * @param fd The event handler to look for
	 * @return True if this fd has an event handler
	 */
	bool HasFd(int fd);

	/** Returns the EventHandler attached to a specific fd.
	 * If the fd isnt in the socketengine, returns NULL.
	 * @param fd The event handler to look for
	 * @return A pointer to the event handler, or NULL
	 */
	EventHandler* GetRef(int fd);

	/** Waits for events and dispatches them to handlers.
	 * Please note that this doesnt wait long, only
	 * a couple of milliseconds. It returns the number of
	 * events which occured during this call.
	 * This method will dispatch events to their handlers
	 * by calling their EventHandler::HandleEvent()
	 * methods with the neccessary EventType value.
	 * @return The number of events which have occured.
	 */
	virtual int DispatchEvents();

	/** Returns the socket engines name.
	 * This returns the name of the engine for use
	 * in /VERSION responses.
	 * @return The socket engine name
	 */
	virtual std::string GetName();
};

#endif
