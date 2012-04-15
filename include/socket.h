/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef SOCKET_H
#define SOCKET_H

#ifndef WIN32

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#endif

#include <cerrno>
#include <vector>
#include <string>
#include <map>

/* Contains irc-specific definitions */
namespace irc
{
	/** This namespace contains various protocol-independent helper classes.
	 * It also contains some types which are often used by the core and modules
	 * in place of inet_* functions and types.
	 */
	namespace sockets
	{
		union CoreExport sockaddrs
		{
			struct sockaddr sa;
			struct sockaddr_in in4;
			struct sockaddr_in6 in6;
			/** Return the size of the structure for syscall passing */
			int sa_size() const;
			/** Return port number or -1 if invalid */
			int port() const;
			/** Return IP only */
			std::string addr() const;
			/** Return human-readable IP/port pair */
			std::string str() const;
			bool operator==(const sockaddrs& other) const;
			inline bool operator!=(const sockaddrs& other) const { return !(*this == other); }
		};

		struct CoreExport cidr_mask
		{
			/** Type, AF_INET or AF_INET6 */
			unsigned char type;
			/** Length of the mask in bits (0-128) */
			unsigned char length;
			/** Raw bits. Unused bits must be zero */
			unsigned char bits[16];

			cidr_mask() {}
			/** Construct a CIDR mask from the string. Will normalize (127.0.0.1/8 => 127.0.0.0/8). */
			cidr_mask(const std::string& mask);
			/** Construct a CIDR mask of a given length from the given address */
			cidr_mask(const irc::sockets::sockaddrs& addr, int len);
			/** Equality of bits, type, and length */
			bool operator==(const cidr_mask& other) const;
			/** Ordering defined for maps */
			bool operator<(const cidr_mask& other) const;
			/** Match within this CIDR? */
			bool match(const irc::sockets::sockaddrs& addr) const;
			/** Convert to a user-readable string.
			 * This means IPv6 addresses are written as [::1]:6667, and *:6668 is used for 0.0.0.0:6668
			 * @return The string; "<unknown>" if not a valid address
			 */
			std::string str() const;
		};

		/** Match CIDR, including an optional username/nickname part.
		 *
		 * This function will compare a human-readable address (plus
		 * optional username and nickname) against a human-readable
		 * CIDR mask, for example joe!bloggs\@1.2.3.4 against
		 * *!bloggs\@1.2.0.0/16. This method supports both IPV4 and
		 * IPV6 addresses.
		 * @param address The human readable address, e.g. fred\@1.2.3.4
		 * @param cidr_mask The human readable mask, e.g. *\@1.2.0.0/16
		 * @return True if the mask matches the address
		 */
		CoreExport bool MatchCIDR(const std::string &address, const std::string &cidr_mask, bool match_with_username);

		/** Return the size of the structure for syscall passing */
		inline int sa_size(const irc::sockets::sockaddrs& sa) { return sa.sa_size(); }

		/** Convert an address-port pair into a binary sockaddr
		 * @param addr The IP address, IPv4 or IPv6
		 * @param port The port, 0 for unspecified
		 * @param sa The structure to place the result in. Will be zeroed prior to conversion
		 * @return true if the conversion was successful, false if not.
		 */
		CoreExport bool aptosa(const std::string& addr, int port, irc::sockets::sockaddrs& sa);
	}
}


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

/**
 * Event mask for SocketEngine events
 */
enum EventMask
{
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

	/** Do not test this socket for writeability
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
	 * Before calling HandleEvent, a socket engine MAY change the state of
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
	 * return to the FD_WANT_NO_WRITE state before HandleEvent is called.
	 */
	FD_WANT_SINGLE_WRITE = 0x100,

	/** Mask for all write events */
	FD_WANT_WRITE_MASK = 0x1F0,

	/** Add a trial read. During the next DispatchEvents invocation, this
	 * will call HandleEvent with EVENT_READ unless reads are known to be
	 * blocking.
	 */
	FD_ADD_TRIAL_READ  = 0x1000,
	/** Assert that reads are known to block. This cancels FD_ADD_TRIAL_READ.
	 * Reset by SE before running EVENT_READ
	 */
	FD_READ_WILL_BLOCK = 0x2000,

	/** Add a trial write. During the next DispatchEvents invocation, this
	 * will call HandleEvent with EVENT_WRITE unless writes are known to be
	 * blocking.
	 * 
	 * This could be used to group several writes together into a single
	 * send() syscall, or to ensure that writes are blocking when attempting
	 * to use FD_WANT_FAST_WRITE.
	 */
	FD_ADD_TRIAL_WRITE = 0x4000,
	/** Assert that writes are known to block. This cancels FD_ADD_TRIAL_WRITE.
	 * Reset by SE before running EVENT_WRITE
	 */
	FD_WRITE_WILL_BLOCK = 0x8000, 

	/** Mask for trial read/trial write */
	FD_TRIAL_NOTE_MASK = 0x5000
};

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
class CoreExport EventHandler : public classbase
{
 private:
	/** Private state maintained by socket engine */
	int event_mask;
 protected:
	/** File descriptor.
	 * All events which can be handled must have a file descriptor.  This
	 * allows you to add events for sockets, fifo's, pipes, and various
	 * other forms of IPC.  Do not change this while the object is
	 * registered with the SocketEngine
	 */
	int fd;
 public:
	/** Get the current file descriptor
	 * @return The file descriptor of this handler
	 */
	inline int GetFd() const { return fd; }

	inline int GetEventMask() const { return event_mask; }

	/** Set a new file desciptor
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

	/** Process an I/O event.
	 * You MUST implement this function in your derived
	 * class, and it will be called whenever read or write
	 * events are received.
	 * @param et either one of EVENT_READ for read events,
	 * and EVENT_WRITE for write events.
	 */
	virtual void HandleEvent(EventType et, int errornum = 0) = 0;

	friend class SocketEngine;
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
class CoreExport SocketEngine
{
 protected:
	/** Current number of descriptors in the engine
	 */
	int CurrentSetSize;
	/** Reference table, contains all current handlers
	 */
	EventHandler** ref;
	/** List of handlers that want a trial read/write
	 */
	std::set<int> trials;

	int MAX_DESCRIPTORS;

	size_t indata;
	size_t outdata;
	time_t lastempty;

	void UpdateStats(size_t len_in, size_t len_out);

	virtual void OnSetEvent(EventHandler* eh, int old_mask, int new_mask) = 0;
	void SetEventMask(EventHandler* eh, int value) const;
public:

	unsigned long TotalEvents;
	unsigned long ReadEvents;
	unsigned long WriteEvents;
	unsigned long ErrorEvents;

	/** Constructor.
	 * The constructor transparently initializes
	 * the socket engine which the ircd is using.
	 * Please note that if there is a catastrophic
	 * failure (for example, you try and enable
	 * epoll on a 2.4 linux kernel) then this
	 * function may bail back to the shell.
	 */
	SocketEngine();

	/** Destructor.
	 * The destructor transparently tidies up
	 * any resources used by the socket engine.
	 */
	virtual ~SocketEngine();

	/** Add an EventHandler object to the engine.  Use AddFd to add a file
	 * descriptor to the engine and have the socket engine monitor it. You
	 * must provide an object derived from EventHandler which implements
	 * HandleEvent().
	 * @param eh An event handling object to add
	 * @param event_mask The initial event mask for the object
	 */
	virtual bool AddFd(EventHandler* eh, int event_mask) = 0;

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
	void ChangeEventMask(EventHandler* eh, int event_mask);

	/** Returns the highest file descriptor you may store in the socket engine
	 * @return The maximum fd value
	 */
	inline int GetMaxFds() const { return MAX_DESCRIPTORS; }

	/** Returns the number of file descriptors being queried
	 * @return The set size
	 */
	inline int GetUsedFds() const { return CurrentSetSize; }

	/** Delete an event handler from the engine.
	 * This function call deletes an EventHandler
	 * from the engine, returning true if it succeeded
	 * and false if it failed. This does not free the
	 * EventHandler pointer using delete, if this is
	 * required you must do this yourself.
	 * @param eh The event handler object to remove
	 */
	virtual void DelFd(EventHandler* eh) = 0;

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

	/** Waits for events and dispatches them to handlers.  Please note that
	 * this doesn't wait long, only a couple of milliseconds. It returns the
	 * number of events which occurred during this call.  This method will
	 * dispatch events to their handlers by calling their
	 * EventHandler::HandleEvent() methods with the necessary EventType
	 * value.
	 * @return The number of events which have occured.
	 */
	virtual int DispatchEvents() = 0;

	/** Dispatch trial reads and writes. This causes the actual socket I/O
	 * to happen when writes have been pre-buffered.
	 */
	virtual void DispatchTrialWrites();

	/** Returns the socket engines name.  This returns the name of the
	 * engine for use in /VERSION responses.
	 * @return The socket engine name
	 */
	virtual std::string GetName() = 0;

	/** Returns true if the file descriptors in the given event handler are
	 * within sensible ranges which can be handled by the socket engine.
	 */
	virtual bool BoundsCheckFd(EventHandler* eh);

	/** Abstraction for BSD sockets accept(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Accept(EventHandler* fd, sockaddr *addr, socklen_t *addrlen);

	/** Abstraction for BSD sockets close(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Close(EventHandler* fd);

	/** Abstraction for BSD sockets close(2).
	 * This function should emulate its namesake system call exactly.
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Close(int fd);

	/** Abstraction for BSD sockets send(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Send(EventHandler* fd, const void *buf, size_t len, int flags);

	/** Abstraction for BSD sockets recv(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Recv(EventHandler* fd, void *buf, size_t len, int flags);

	/** Abstraction for BSD sockets recvfrom(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int RecvFrom(EventHandler* fd, void *buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen);

	/** Abstraction for BSD sockets sendto(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int SendTo(EventHandler* fd, const void *buf, size_t len, int flags, const sockaddr *to, socklen_t tolen);

	/** Abstraction for BSD sockets connect(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Connect(EventHandler* fd, const sockaddr *serv_addr, socklen_t addrlen);

	/** Make a file descriptor blocking.
	 * @param fd a file descriptor to set to blocking mode
	 * @return 0 on success, -1 on failure, errno is set appropriately.
	 */
	int Blocking(int fd);

	/** Make a file descriptor nonblocking.
	 * @param fd A file descriptor to set to nonblocking mode
	 * @return 0 on success, -1 on failure, errno is set appropriately.
	 */
	int NonBlocking(int fd);

	/** Abstraction for BSD sockets shutdown(2).
	 * This function should emulate its namesake system call exactly.
	 * @param fd This version of the call takes an EventHandler instead of a bare file descriptor.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Shutdown(EventHandler* fd, int how);

	/** Abstraction for BSD sockets shutdown(2).
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Shutdown(int fd, int how);

	/** Abstraction for BSD sockets bind(2).
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Bind(int fd, const irc::sockets::sockaddrs& addr);

	/** Abstraction for BSD sockets listen(2).
	 * This function should emulate its namesake system call exactly.
	 * @return This method should return exactly the same values as the system call it emulates.
	 */
	int Listen(int sockfd, int backlog);

	/** Set SO_REUSEADDR and SO_LINGER on this file descriptor
	 */
	void SetReuse(int sockfd);

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

SocketEngine* CreateSocketEngine();

/** This class handles incoming connections on client ports.
 * It will create a new User for every valid connection
 * and assign it a file descriptor.
 */
class CoreExport ListenSocket : public EventHandler
{
 public:
	const reference<ConfigTag> bind_tag;
	std::string bind_addr;
	int bind_port;
	/** Human-readable bind description */
	std::string bind_desc;
	/** Create a new listening socket
	 */
	ListenSocket(ConfigTag* tag, const irc::sockets::sockaddrs& bind_to);
	/** Handle an I/O event
	 */
	void HandleEvent(EventType et, int errornum = 0);
	/** Close the socket
	 */
	~ListenSocket();

	/** Handles sockets internals crap of a connection, convenience wrapper really
	 */
	void AcceptInternal();
};

#endif

