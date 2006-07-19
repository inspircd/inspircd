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
#include "globals.h"
#include "inspircd.h"

/**
 * Each of these values represents a socket
 * type in our reference table (the reference
 * table itself is only accessible to
 * socketengine.cpp)
 */
const char X_EMPTY_SLOT		= 0;
const char X_LISTEN             = 1;
const char X_ESTAB_CLIENT       = 2;
const char X_ESTAB_MODULE       = 3;
const char X_ESTAB_DNS          = 4;
const char X_ESTAB_CLASSDNS	= 5;

/**
 * To indicate that a socket is readable, we
 * mask its top bit with this X_READBIT value.
 * The socket engine can handle two types of
 * socket, readable and writeable (error sockets
 * are dealt with when read() and write() return
 * negative or zero values).
 */
const char X_READBIT            = 0x80;

/**
 * The actual socketengine class presents the
 * same interface on all operating systems, but
 * its private members and internal behaviour
 * should be treated as blackboxed, and vary
 * from system to system and upon the config
 * settings chosen by the server admin. The current
 * version supports select, epoll and kqueue.
 */
class SocketEngine : public Extensible
{
protected:
	int EngineHandle;			/* Handle to the socket engine if needed */
	int CurrentSetSize;			/* Current number of descriptors in the engine */
	char ref[MAX_DESCRIPTORS];		/* Reference table */
public:

	/** Constructor
	 * The constructor transparently initializes
	 * the socket engine which the ircd is using.
	 * Please note that if there is a catastrophic
	 * failure (for example, you try and enable
	 * epoll on a 2.4 linux kernel) then this
	 * function may bail back to the shell.
	 */
	SocketEngine();

	/** Destructor
	 * The destructor transparently tidies up
	 * any resources used by the socket engine.
	 */
	virtual ~SocketEngine();

	/** Add a file descriptor to the engine
	 * Use AddFd to add a file descriptor to the
	 * engine and have the socket engine monitor
	 * it. You must provide a type (see the consts
	 * in socketengine.h) and a boolean flag to
	 * indicate wether to watch this fd for read
	 * or write events (there is currently no
	 * need for support of both).
	 */
	virtual bool AddFd(int fd, bool readable, char type);

	/** Returns the type value for this file descriptor
	 * This function masks off the X_READBIT value
	 * so that the type of the socket can be obtained.
	 * The core uses this to decide where to dispatch
	 * the event to. Please note that some engines
	 * such as select() have an upper limit of 1024
	 * descriptors which may be active at any one time,
	 * where others such as kqueue have no practical
	 * limits at all.
	 */
	char GetType(int fd);

	/** Returns the maximum number of file descriptors
	 * you may store in the socket engine at any one time.
	 */
	virtual int GetMaxFds();

	/** Returns the number of file descriptor slots
	 * which are available for storing fds.
	 */
	virtual int GetRemainingFds();

	/** Delete a file descriptor from the engine
	 * This function call deletes a file descriptor
	 * from the engine, returning true if it succeeded
	 * and false if it failed.
	 */
	virtual bool DelFd(int fd);

	/** Returns true if a socket exists in the socket
	 * engine's list.
	 */
	bool HasFd(int fd);

	/** Waits for an event.
	 * Please note that this doesnt wait long, only
	 * a couple of milliseconds. It returns a list
	 * of active file descriptors in the vector
	 * fdlist which the core may then act upon.
	 */
	virtual int Wait(int* fdlist);

	/** Returns the socket engines name
	 * This returns the name of the engine for use
	 * in /VERSION responses.
	 */
	virtual std::string GetName();
};

#endif
