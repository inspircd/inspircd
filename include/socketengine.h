/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
 * EVENT_ERROR can always occur, and indicates
 * a write error or read error on the socket,
 * e.g. EOF condition or broken pipe.
 */
enum EventType
{
	/** Read event */
	EVENT_READ	=	0,
	/** Write event */
	EVENT_WRITE	=	1,
	/** Error event */
	EVENT_ERROR	=	2
};

class InspIRCd;
class Module;

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
class CoreExport EventHandler : public Extensible
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

	/** Pointer to the module which has hooked the given EventHandler for IO events.
	 */
	Module *IOHook;
 public:

	/** Return the current hooker of IO events for this socket, or NULL.
	 * @return Hooker module, if set, or NULL.
	 */
	Module *GetIOHook();

	/** Set a module as hooking IO events on this socket.
	 * @param IOHooker The module hooking IO
	 * @return True if the hook could be added, false otherwise.
	 */
	bool AddIOHook(Module *IOHooker);

	/** Remove IO hooking from a module
	 * @return True if hooking was successfully removed, false otherwise.
	 */
	bool DelIOHook();

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
	EventHandler();

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
	 *
	 * NOTE: You cannot set both Readable() and
	 * Writeable() to true. If you wish to receive
	 * a write event for your object, you must call
	 * SocketEngine::WantWrite() instead. This will
	 * trigger your objects next EVENT_WRITE type event.
	 */
	virtual bool Readable();

	/** Override this function to indicate writeability.
	 * @return This should return true if the function
	 * wishes to receive EVENT_WRITE events. Do not change
	 * what this function returns while the event handler
	 * is still added to a SocketEngine instance!
	 * If this function is unimplemented, the base class
	 * will return false.
	 *
	 * NOTE: You cannot set both Readable() and
	 * Writeable() to true. If you wish to receive
	 * a write event for your object, you must call
	 * SocketEngine::WantWrite() instead. This will
	 * trigger your objects next EVENT_WRITE type event.
	 */
	virtual bool Writeable();

	/** Process an I/O event.
	 * You MUST implement this function in your derived
	 * class, and it will be called whenever read or write
	 * events are received, depending on what your functions
	 * Readable() and Writeable() returns and wether you
	 * previously made a call to SocketEngine::WantWrite().
	 * @param et either one of EVENT_READ for read events,
	 * and EVENT_WRITE for write events.
	 */
	virtual void HandleEvent(EventType et, int errornum = 0) = 0;
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
class CoreExport SocketEngine : public Extensible
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
	EventHandler** ref;

	int MAX_DESCRIPTORS;

	size_t indata;
	size_t outdata;
	time_t lastempty;

	void UpdateStats(size_t len_in, size_t len_out);
public:

	double TotalEvents;
	double ReadEvents;
	double WriteEvents;
	double ErrorEvents;

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

	/** If you call this function and pass it an
	 * event handler, that event handler will
	 * receive the next available write event,
	 * even if the socket is a readable socket only.
	 * Developers should avoid constantly keeping
	 * an eventhandler in the writeable state,
	 * as this will consume large amounts of
	 * CPU time.
	 * @param eh An event handler which wants to
	 * receive the next writeability event.
	 */
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
	 * Note on forcing deletes. DO NOT DO THIS! This is
	 * extremely dangerous and will most likely render the
	 * socketengine dead. This was added only for handling
	 * very rare cases where broken 3rd party libs destroys
	 * the OS socket beyond our control. If you can't explain
	 * in minute details why forcing is absolutely necessary
	 * then you don't need it. That was a NO!
	 * @param eh The event handler object to remove
	 * @param force *DANGEROUS* See method description!
	 * @return True if the event handler was removed
	 */
	virtual bool DelFd(EventHandler* eh, bool force = false);

	/** Returns true if a file descriptor exists in
	 * the socket engine's list.
	 * @param fd The event handler to look for
	 * @return True if this fd has an event handler
	 */
	virtual bool HasFd(int fd);

	/** Returns the EventHandler attached to a specific fd.
	 * If the fd isnt in the socketengine, returns NULL.
	 * @param fd The event handler to look for
	 * @return A pointer to the event handler, or NULL
	 */
	virtual EventHandler* GetRef(int fd);

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

	/** Returns true if the file descriptors in the
	 * given event handler are within sensible ranges
	 * which can be handled by the socket engine.
	 */
	virtual bool BoundsCheckFd(EventHandler* eh);

	/** Abstraction for BSD sockets accept(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen);

	/** Abstraction for BSD sockets close(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Close(EventHandler* fd);

	/** Abstraction for BSD sockets close(2).
	 * This function should emulate its namesake system call exactly.
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Close(int fd);

	/** Abstraction for BSD sockets send(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Send(EventHandler* fd, const void *buf, size_t len, int flags);

	/** Abstraction for BSD sockets recv(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Recv(EventHandler* fd, void *buf, size_t len, int flags);

	/** Abstraction for BSD sockets recvfrom(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen);

	/** Abstraction for BSD sockets sendto(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int SendTo(EventHandler* fd, const void *buf, size_t len, int flags, const sockaddr *to, socklen_t tolen);

	/** Abstraction for BSD sockets connect(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Connect(EventHandler* fd, const sockaddr *serv_addr, socklen_t addrlen);

	/** Make a file descriptor blocking.
	 * @param fd a file descriptor to set to blocking mode
	 * @return 0 on success, -1 on failure, errno is set appropriately.
	 */
	virtual int Blocking(int fd);

	/** Make a file descriptor nonblocking.
	 * @param fd A file descriptor to set to nonblocking mode
	 * @return 0 on success, -1 on failure, errno is set appropriately.
	 */
	virtual int NonBlocking(int fd);

	/** Abstraction for BSD sockets shutdown(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Shutdown(EventHandler* fd, int how);

	/** Abstraction for BSD sockets shutdown(2).
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Shutdown(int fd, int how);

	/** Abstraction for BSD sockets bind(2).
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Bind(int fd, const sockaddr *my_addr, socklen_t addrlen);

	/** Abstraction for BSD sockets listen(2).
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int Listen(int sockfd, int backlog);

	/** Abstraction for BSD sockets getsockname(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	virtual int GetSockName(EventHandler* fd, sockaddr *name, socklen_t* namelen);

	/** This function is called immediately after fork().
	 * Some socket engines (notably kqueue) cannot have their
	 * handles inherited by forked processes. This method
	 * allows for the socket engine to re-create its handle
	 * after the daemon forks as the socket engine is created
	 * long BEFORE the daemon forks.
	 * @return void, but it is acceptable for this function to bail back to
	 * the shell or operating system on fatal error.
	 */
	virtual void RecoverFromFork();

	/** Get data transfer statistics, kilobits per second in and out and total.
	 */
	void GetStats(float &kbitpersec_in, float &kbitpersec_out, float &kbitpersec_total);
};

#endif

