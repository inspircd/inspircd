/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2014 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008, 2017 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

#include <string>
#include "socket.h"
#include "base.h"

#ifndef _WIN32
#include <sys/uio.h>
#endif

#ifndef IOV_MAX
#define IOV_MAX 1024
#endif

/**
 * Event mask for SocketEngine events
 */
enum EventMask {
    /** Do not test this socket for readability
     */
    FD_WANT_NO_READ = 0x1,
    /** Give a read event at all times when reads will not block.
     */
    FD_WANT_POLL_READ = 0x2,
    /** Give a read event when there is new data to read.
     *
     * An event MUST be sent if there is new data to be read, and the most
     * recent read/recv() on this FD returned EAGAIN. An event MAY be sent
     * at any time there is data to be read on the socket.
     */
    FD_WANT_FAST_READ = 0x4,
    /** Give an optional read event when reads begin to unblock
     *
     * This state is useful if you want to leave data in the OS receive
     * queue but not get continuous event notifications about it, because
     * it may not require a system call to transition from FD_WANT_FAST_READ
     */
    FD_WANT_EDGE_READ = 0x8,

    /** Mask for all read events */
    FD_WANT_READ_MASK = 0x0F,

    /** Do not test this socket for writability
     */
    FD_WANT_NO_WRITE = 0x10,
    /** Give a write event at all times when writes will not block.
     *
     * You probably shouldn't use this state; if it's likely that the write
     * will not block, try it first, then use FD_WANT_FAST_WRITE if it
     * fails. If it's likely to block (or you are using polling-style reads)
     * then use FD_WANT_SINGLE_WRITE.
     */
    FD_WANT_POLL_WRITE = 0x20,
    /** Give a write event when writes don't block any more
     *
     * An event MUST be sent if writes will not block, and the most recent
     * write/send() on this FD returned EAGAIN, or connect() returned
     * EINPROGRESS. An event MAY be sent at any time that writes will not
     * block.
     *
     * Before calling OnEventHandler*(), a socket engine MAY change the state of
     * the FD back to FD_WANT_EDGE_WRITE if it is simpler (for example, if a
     * one-shot notification was registered). If further writes are needed,
     * it is the responsibility of the event handler to change the state to
     * one that will generate the required notifications
     */
    FD_WANT_FAST_WRITE = 0x40,
    /** Give an optional write event on edge-triggered write unblock.
     *
     * This state is useful to avoid system calls when moving to/from
     * FD_WANT_FAST_WRITE when writing data to a mostly-unblocked socket.
     */
    FD_WANT_EDGE_WRITE = 0x80,
    /** Request a one-shot poll-style write notification. The socket will
     * return to the FD_WANT_NO_WRITE state before OnEventHandler*() is called.
     */
    FD_WANT_SINGLE_WRITE = 0x100,

    /** Mask for all write events */
    FD_WANT_WRITE_MASK = 0x1F0,

    /** Add a trial read. During the next DispatchEvents invocation, this
     * will call OnEventHandlerRead() unless reads are known to be
     * blocking.
     */
    FD_ADD_TRIAL_READ  = 0x1000,
    /** Assert that reads are known to block. This cancels FD_ADD_TRIAL_READ.
     * Reset by SE before running OnEventHandlerRead().
     */
    FD_READ_WILL_BLOCK = 0x2000,

    /** Add a trial write. During the next DispatchEvents invocation, this
     * will call OnEventHandlerWrite() unless writes are known to be
     * blocking.
     *
     * This could be used to group several writes together into a single
     * send() syscall, or to ensure that writes are blocking when attempting
     * to use FD_WANT_FAST_WRITE.
     */
    FD_ADD_TRIAL_WRITE = 0x4000,
    /** Assert that writes are known to block. This cancels FD_ADD_TRIAL_WRITE.
     * Reset by SE before running OnEventHandlerWrite().
     */
    FD_WRITE_WILL_BLOCK = 0x8000,

    /** Mask for trial read/trial write */
    FD_TRIAL_NOTE_MASK = 0x5000
};

/** This class is a basic I/O handler class.
 * Any object which wishes to receive basic I/O events
 * from the socketengine must derive from this class and
 * implement the OnEventHandler*() methods. The derived class
 * must then be added to SocketEngine using the method
 * SocketEngine::AddFd(), after which point the derived
 * class will receive events to its OnEventHandler*() methods.
 * The event mask passed to SocketEngine::AddFd() determines
 * what events the EventHandler gets notified about and with
 * what semantics. SocketEngine::ChangeEventMask() can be
 * called to update the event mask later. The only
 * requirement beyond this for an event handler is that it
 * must have a file descriptor. What this file descriptor
 * is actually attached to is completely up to you.
 */
class CoreExport EventHandler : public classbase {
  private:
    /** Private state maintained by socket engine */
    int event_mask;

    void SetEventMask(int mask) {
        event_mask = mask;
    }

  protected:
    /** File descriptor.
     * All events which can be handled must have a file descriptor.  This
     * allows you to add events for sockets, fifo's, pipes, and various
     * other forms of IPC.  Do not change this while the object is
     * registered with the SocketEngine
     */
    int fd;

    /** Swaps the internals of this EventHandler with another one.
     * @param other A EventHandler to swap internals with.
     */
    void SwapInternals(EventHandler& other);

  public:
    /** Get the current file descriptor
     * @return The file descriptor of this handler
     */
    inline int GetFd() const {
        return fd;
    }

    /** Checks if this event handler has a fd associated with it. */
    inline bool HasFd() const {
        return fd >= 0;
    }

    inline int GetEventMask() const {
        return event_mask;
    }

    /** Set a new file descriptor
     * @param FD The new file descriptor. Do not call this method without
     * first deleting the object from the SocketEngine if you have
     * added it to a SocketEngine instance.
     */
    void SetFd(int FD);

    /** Constructor
     */
    EventHandler();

    /** Destructor
     */
    virtual ~EventHandler() {}

    /** Called by the socket engine in case of a read event
     */
    virtual void OnEventHandlerRead() = 0;

    /** Called by the socket engine in case of a write event.
     * The default implementation does nothing.
     */
    virtual void OnEventHandlerWrite();

    /** Called by the socket engine in case of an error event.
     * The default implementation does nothing.
     * @param errornum Error code
     */
    virtual void OnEventHandlerError(int errornum);

    friend class SocketEngine;
};

/** Provides basic file-descriptor-based I/O support.
 * The actual socketengine class presents the
 * same interface on all operating systems, but
 * its private members and internal behaviour
 * should be treated as blackboxed, and vary
 * from system to system and upon the config
 * settings chosen by the server admin.
 */
class CoreExport SocketEngine {
  public:
    /** Socket engine statistics: count of various events, bandwidth usage
     */
    class Statistics {
        mutable size_t indata;
        mutable size_t outdata;
        mutable time_t lastempty;

        /** Reset the byte counters and lastempty if there wasn't a reset in this second.
         */
        void CheckFlush() const;

      public:
        /** Constructor, initializes member vars except indata and outdata because those are set to 0
         * in CheckFlush() the first time Update() or GetBandwidth() is called.
         */
        Statistics() : lastempty(0), TotalEvents(0), ReadEvents(0), WriteEvents(0),
            ErrorEvents(0) { }

        /** Update counters for network data received.
         * This should be called after every read-type syscall.
         * @param len_in Number of bytes received, or -1 for error, as typically
         * returned by a read-style syscall.
         */
        void UpdateReadCounters(int len_in);

        /** Update counters for network data sent.
         * This should be called after every write-type syscall.
         * @param len_out Number of bytes sent, or -1 for error, as typically
         * returned by a read-style syscall.
         */
        void UpdateWriteCounters(int len_out);

        /** Get data transfer statistics.
         * @param kbitpersec_in Filled with incoming traffic in this second in kbit/s.
         * @param kbitpersec_out Filled with outgoing traffic in this second in kbit/s.
         * @param kbitpersec_total Filled with total traffic in this second in kbit/s.
         */
        void CoreExport GetBandwidth(float& kbitpersec_in, float& kbitpersec_out,
                                     float& kbitpersec_total) const;

        unsigned long TotalEvents;
        unsigned long ReadEvents;
        unsigned long WriteEvents;
        unsigned long ErrorEvents;
    };

  private:
    /** Reference table, contains all current handlers
     **/
    static std::vector<EventHandler*> ref;

    /** Current number of descriptors in the engine. */
    static size_t CurrentSetSize;

    /** The maximum number of descriptors in the engine. */
    static size_t MaxSetSize;

    /** List of handlers that want a trial read/write
     */
    static std::set<int> trials;

    /** Socket engine statistics: count of various events, bandwidth usage
     */
    static Statistics stats;

    /** Look up the fd limit using rlimit. */
    static void LookupMaxFds();

    /** Terminates the program when the socket engine fails to initialize. */
    static void InitError();

    static void OnSetEvent(EventHandler* eh, int old_mask, int new_mask);

    /** Add an event handler to the base socket engine. AddFd(EventHandler*, int) should call this.
     */
    static bool AddFdRef(EventHandler* eh);

    static void DelFdRef(EventHandler* eh);

    template <typename T>
    static void ResizeDouble(std::vector<T>& vect) {
        if (SocketEngine::CurrentSetSize > vect.size()) {
            vect.resize(SocketEngine::CurrentSetSize * 2);
        }
    }

  public:
#ifndef _WIN32
    typedef iovec IOVector;
#else
    typedef WindowsIOVec IOVector;
#endif

    /** Constructor.
     * The constructor transparently initializes
     * the socket engine which the ircd is using.
     * Please note that if there is a catastrophic
     * failure (for example, you try and enable
     * epoll on a 2.4 linux kernel) then this
     * function may bail back to the shell.
     */
    static void Init();

    /** Destructor.
     * The destructor transparently tidies up
     * any resources used by the socket engine.
     */
    static void Deinit();

    /** Add an EventHandler object to the engine.  Use AddFd to add a file
     * descriptor to the engine and have the socket engine monitor it. You
     * must provide an object derived from EventHandler which implements
     * the required OnEventHandler*() methods.
     * @param eh An event handling object to add
     * @param event_mask The initial event mask for the object
     */
    static bool AddFd(EventHandler* eh, int event_mask);

    /** If you call this function and pass it an
     * event handler, that event handler will
     * receive the next available write event,
     * even if the socket is a readable socket only.
     * Developers should avoid constantly keeping
     * an eventhandler in the writeable state,
     * as this will consume large amounts of
     * CPU time.
     * @param eh The event handler to change
     * @param event_mask The changes to make to the wait state
     */
    static void ChangeEventMask(EventHandler* eh, int event_mask);

    /** Returns the number of file descriptors reported by the system this program may use
     * when it was started.
     * @return If non-zero the number of file descriptors that the system reported that we
     * may use.
     */
    static size_t GetMaxFds() {
        return MaxSetSize;
    }

    /** Returns the number of file descriptors being queried
     * @return The set size
     */
    static size_t GetUsedFds() {
        return CurrentSetSize;
    }

    /** Delete an event handler from the engine.
     * This function call deletes an EventHandler
     * from the engine, returning true if it succeeded
     * and false if it failed. This does not free the
     * EventHandler pointer using delete, if this is
     * required you must do this yourself.
     * @param eh The event handler object to remove
     */
    static void DelFd(EventHandler* eh);

    /** Returns true if a file descriptor exists in
     * the socket engine's list.
     * @param fd The event handler to look for
     * @return True if this fd has an event handler
     */
    static bool HasFd(int fd);

    /** Returns the EventHandler attached to a specific fd.
     * If the fd isn't in the socketengine, returns NULL.
     * @param fd The event handler to look for
     * @return A pointer to the event handler, or NULL
     */
    static EventHandler* GetRef(int fd);

    /** Waits for events and dispatches them to handlers.  Please note that
     * this doesn't wait long, only a couple of milliseconds. It returns the
     * number of events which occurred during this call.  This method will
     * dispatch events to their handlers by calling their
     * EventHandler::OnEventHandler*() methods.
     * @return The number of events which have occurred.
     */
    static int DispatchEvents();

    /** Dispatch trial reads and writes. This causes the actual socket I/O
     * to happen when writes have been pre-buffered.
     */
    static void DispatchTrialWrites();

    /** Returns true if the file descriptors in the given event handler are
     * within sensible ranges which can be handled by the socket engine.
     */
    static bool BoundsCheckFd(EventHandler* eh);

    /** Abstraction for BSD sockets accept(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param addr The client IP address and port
     * @param addrlen The size of the sockaddr parameter.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen);

    /** Close the underlying fd of an event handler, remove it from the socket engine and set the fd to -1.
     * @param eh The EventHandler to close.
     * @return 0 on success, a negative value on error
     */
    static int Close(EventHandler* eh);

    /** Abstraction for BSD sockets close(2).
     * This function should emulate its namesake system call exactly.
     * This function should emulate its namesake system call exactly.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Close(int fd);

    /** Abstraction for BSD sockets send(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param buf The buffer in which the data that is sent is stored.
     * @param len The size of the buffer.
     * @param flags A flag value that controls the sending of the data.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Send(EventHandler* fd, const void *buf, size_t len, int flags);

    /** Abstraction for vector write function writev().
     * This function should emulate its namesake system call exactly.
     * @param fd EventHandler to send data with
     * @param iov Array of IOVectors containing the buffers to send and their lengths in the platform's
     * native format.
     * @param count Number of elements in iov.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int WriteV(EventHandler* fd, const IOVector* iov, int count);

#ifdef _WIN32
    /** Abstraction for vector write function writev() that accepts a POSIX format iovec.
     * This function should emulate its namesake system call exactly.
     * @param fd EventHandler to send data with
     * @param iov Array of iovecs containing the buffers to send and their lengths in POSIX format.
     * @param count Number of elements in iov.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int WriteV(EventHandler* fd, const iovec* iov, int count);
#endif

    /** Abstraction for BSD sockets recv(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param buf The buffer in which the data that is read is stored.
     * @param len The size of the buffer.
     * @param flags A flag value that controls the reception of the data.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Recv(EventHandler* fd, void *buf, size_t len, int flags);

    /** Abstraction for BSD sockets recvfrom(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param buf The buffer in which the data that is read is stored.
     * @param len The size of the buffer.
     * @param flags A flag value that controls the reception of the data.
     * @param from The remote IP address and port.
     * @param fromlen The size of the from parameter.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int RecvFrom(EventHandler* fd, void *buf, size_t len, int flags,
                        sockaddr *from, socklen_t *fromlen);

    /** Abstraction for BSD sockets sendto(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param buf The buffer in which the data that is sent is stored.
     * @param len The size of the buffer.
     * @param flags A flag value that controls the sending of the data.
     * @param address The remote IP address and port.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int SendTo(EventHandler* fd, const void* buf, size_t len, int flags,
                      const irc::sockets::sockaddrs& address);

    /** Abstraction for BSD sockets connect(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param address The server IP address and port.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Connect(EventHandler* fd, const irc::sockets::sockaddrs& address);

    /** Make a file descriptor blocking.
     * @param fd a file descriptor to set to blocking mode
     * @return 0 on success, -1 on failure, errno is set appropriately.
     */
    static int Blocking(int fd);

    /** Make a file descriptor nonblocking.
     * @param fd A file descriptor to set to nonblocking mode
     * @return 0 on success, -1 on failure, errno is set appropriately.
     */
    static int NonBlocking(int fd);

    /** Abstraction for BSD sockets shutdown(2).
     * This function should emulate its namesake system call exactly.
     * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
     * @param how What part of the socket to shut down
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Shutdown(EventHandler* fd, int how);

    /** Abstraction for BSD sockets shutdown(2).
     * This function should emulate its namesake system call exactly.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Shutdown(int fd, int how);

    /** Abstraction for BSD sockets bind(2).
     * This function should emulate its namesake system call exactly.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Bind(int fd, const irc::sockets::sockaddrs& addr);

    /** Abstraction for BSD sockets listen(2).
     * This function should emulate its namesake system call exactly.
     * @return This method should return exactly the same values as the system call it emulates.
     */
    static int Listen(int sockfd, int backlog);

    /** Set SO_REUSEADDR and SO_LINGER on this file descriptor
     */
    static void SetReuse(int sockfd);

    /** This function is called immediately after fork().
     * Some socket engines (notably kqueue) cannot have their
     * handles inherited by forked processes. This method
     * allows for the socket engine to re-create its handle
     * after the daemon forks as the socket engine is created
     * long BEFORE the daemon forks.
     */
    static void RecoverFromFork();

    /** Get data transfer and event statistics
     */
    static const Statistics& GetStats() {
        return stats;
    }

    /** Should we ignore the error in errno?
     * Checks EAGAIN and WSAEWOULDBLOCK
     */
    static bool IgnoreError();

    /** Return the last socket related error. strrerror(errno) on *nix
     */
    static std::string LastError();

    /** Returns the error for the given error num, strerror(errnum) on *nix
     */
    static std::string GetError(int errnum);
};

inline bool SocketEngine::IgnoreError() {
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        return true;
    }

#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) {
        return true;
    }
#endif

    return false;
}
