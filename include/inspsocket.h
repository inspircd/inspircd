/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __INSP_SOCKET_H__
#define __INSP_SOCKET_H__

#include "timer.h"

/**
 * States which a socket may be in
 */
enum BufferedSocketState
{
	/** Socket disconnected */
	I_DISCONNECTED,
	/** Socket connecting */
	I_CONNECTING,
	/** Socket fully connected */
	I_CONNECTED,
	/** Socket has an error */
	I_ERROR
};

/**
 * Error types which a socket may exhibit
 */
enum BufferedSocketError
{
	/** Socket connect timed out */
	I_ERR_TIMEOUT,
	/** Socket could not be created */
	I_ERR_SOCKET,
	/** Socket could not connect (refused) */
	I_ERR_CONNECT,
	/** Socket could not bind to local port/ip */
	I_ERR_BIND,
	/** Socket could not reslve host (depreciated) */
	I_ERR_RESOLVE,
	/** Socket could not write data */
	I_ERR_WRITE,
	/** No more file descriptors left to create socket! */
	I_ERR_NOMOREFDS
};

/* Required forward declarations */
class BufferedSocket;
class InspIRCd;

/** Used to time out socket connections
 */
class CoreExport SocketTimeout : public Timer
{
 private:
	/** BufferedSocket the class is attached to
	 */
	BufferedSocket* sock;

	/** Server instance creating the timeout class
	 */
	InspIRCd* ServerInstance;

	/** File descriptor of class this is attached to
	 */
	int sfd;

 public:
	/** Create a socket timeout class
	 * @param fd File descriptor of BufferedSocket
	 * @pram Instance server instance to attach to
	 * @param thesock BufferedSocket to attach to
	 * @param secs_from_now Seconds from now to time out
	 * @param now The current time
	 */
	SocketTimeout(int fd, InspIRCd* Instance, BufferedSocket* thesock, long secs_from_now, time_t now) : Timer(secs_from_now, now), sock(thesock), ServerInstance(Instance), sfd(fd) { };

	/** Handle tick event
	 */
	virtual void Tick(time_t now);
};

/**
 * BufferedSocket is an extendable socket class which modules
 * can use for TCP socket support. It is fully integrated
 * into InspIRCds socket loop and attaches its sockets to
 * the core's instance of the SocketEngine class, meaning
 * that all use is fully asynchronous.
 *
 * To use BufferedSocket, you must inherit a class from it.
 */
class CoreExport BufferedSocket : public EventHandler
{
 public:

	/** Bind IP
	 */
	std::string cbindip;

	/** Instance we were created by
	 */
	InspIRCd* ServerInstance;

	/** Timeout class or NULL
	 */
	SocketTimeout* Timeout;

	/** Socket output buffer (binary safe)
	 */
	std::deque<std::string> outbuffer;

	/** The hostname connected to
	 */
	char host[MAXBUF];

	/** The port connected to
	 */
	int port;

	/**
	 * The state for this socket, either
	 * listening, connecting, connected
	 * or error.
	 */
	BufferedSocketState state;

	/**
	 * The IP address being connected
	 * to stored in string form for
	 * easy retrieval by accessors.
	 */
	char IP[MAXBUF];

	/**
	 * Used by accept() to indicate the
	 * sizes of the sockaddr_in structures
	 */
	socklen_t length;

	/** Flushes the write buffer
	 * @returns true if the writing failed, false if it was successful
	 */
	bool FlushWriteBuffer();

	/** Set the queue sizes
	 * This private method sets the operating system queue
	 * sizes for this socket to 65535 so that it can queue
	 * more information without application-level queueing
	 * which was required in older software.
	 */
	void SetQueues();

	/** When the socket has been marked as closing, this flag
	 * will be set to true, then the next time the socket is
	 * examined, the socket is deleted and closed.
	 */
	bool ClosePending;

	/**
	 * Bind to an address
	 * @param ip IP to bind to
	 * @return True is the binding succeeded
	 */
	bool BindAddr(const std::string &ip);

	/** (really) Try bind to a given IP setup. For internal use only.
	 */
	bool DoBindMagic(const std::string &current_ip, bool v6);

	/**
	 * The default constructor does nothing
	 * and should not be used.
	 */
	BufferedSocket(InspIRCd* SI);

	/**
	 * This constructor is used to associate
	 * an existing connecting with an BufferedSocket
	 * class. The given file descriptor must be
	 * valid, and when initialized, the BufferedSocket
	 * will be set with the given IP address
	 * and placed in CONNECTED state.
	 */
	BufferedSocket(InspIRCd* SI, int newfd, const char* ip);

	/**
	 * This constructor is used to create a new outbound connection to another host.
	 * Note that if you specify a hostname in the 'ipaddr' parameter, this class will not
	 * connect. You must resolve your hostnames before passing them to BufferedSocket. To do so,
	 * you should use the nonblocking class 'Resolver'.
	 * @param ipaddr The IP to connect to, or bind to
	 * @param port The port number to connect to
	 * @param maxtime Number of seconds to wait, if connecting, before the connection times out and an OnTimeout() event is generated
	 * @param connectbindip When creating an outbound connection, the IP to bind the connection to. If not defined, the port is not bound.
	 * @return On exit, GetState() returns I_ERROR if an error occured, and errno can be used to read the socket error.
	 */
	BufferedSocket(InspIRCd* SI, const std::string &ipaddr, int port, unsigned long maxtime, const std::string &connectbindip = "");

	/**
	 * This method is called when an outbound
	 * connection on your socket is completed.
	 * @return false to abort the connection, true to continue
	 */
	virtual bool OnConnected();

	/**
	 * This method is called when an error occurs.
	 * A closed socket in itself is not an error,
	 * however errors also generate close events.
	 * @param e The error type which occured
	 */
	virtual void OnError(BufferedSocketError e);

	/**
	 * When an established connection is terminated,
	 * the OnDisconnect method is triggered.
	 */
	virtual int OnDisconnect();

	/**
	 * When there is data waiting to be read on a
	 * socket, the OnDataReady() method is called.
	 * Within this method, you *MUST* call the Read()
	 * method to read any pending data. At its lowest
	 * level, this event is signalled by the core via
	 * the socket engine. If you return false from this
	 * function, the core removes your socket from its
	 * list and erases it from the socket engine, then
	 * calls BufferedSocket::Close() and deletes it.
	 * @return false to close the socket
	 */
	virtual bool OnDataReady();

	/**
	 * When it is ok to write to the socket, and a
	 * write event was requested, this method is
	 * triggered.
	 *
	 * Within this method you should call
	 * write() or send() etc, to send data to the
	 * other end of the socket.
	 *
	 * Further write events will not be triggered
	 * unless you call SocketEngine::WantWrite().
	 *
	 * The default behaviour of this method is to
	 * flush the write buffer, respecting the IO
	 * hooking modules.
	 *
	 * XXX: this used to be virtual, ask us if you need it to be so.
	 * @return false to close the socket
	 */
	bool OnWriteReady();

	/**
	 * When an outbound connection fails, and the
	 * attempt times out, you will receive this event.
	 * The method will trigger once maxtime seconds are
	 * reached (as given in the constructor) just
	 * before the socket's descriptor is closed.
	 * A failed DNS lookup may cause this event if
	 * the DNS server is not responding, as well as
	 * a failed connect() call, because DNS lookups are
	 * nonblocking as implemented by this class.
	 */
	virtual void OnTimeout();

	/**
	 * Whenever close() is called, OnClose() will be
	 * called first. Please note that this means
	 * OnClose will be called alongside OnError(),
	 * OnTimeout(), and Close().
	 */
	virtual void OnClose();

	/**
	 * Reads all pending bytes from the socket
	 * into a char* array which can be up to
	 * 16 kilobytes in length.
	 */
	virtual const char* Read();

	/**
	 * Returns the IP address associated with
	 * this connection, or an empty string if
	 * no IP address exists.
	 */
	std::string GetIP();

	/**
	 * Writes a std::string to the socket. No carriage
	 * returns or linefeeds are appended to the string.
	 * @param data The data to send
	 */
	virtual void Write(const std::string &data);

	/**
	 * Changes the socket's state. The core uses this
	 * to change socket states, and you should not call
	 * it directly.
	 */
	void SetState(BufferedSocketState s);

	/**
	 * Returns the current socket state.
	 */
	BufferedSocketState GetState();

	/** Mark a socket as being connected and call appropriate events.
	 */
	bool InternalMarkConnected();

	/**
	 * This method causes the socket to close, and may
	 * also be triggered by other methods such as OnTimeout
	 * and OnError.
	 */
	virtual void Close();

	/**
	 * The destructor may implicitly call OnClose(), and
	 * will close() and shutdown() the file descriptor
	 * used for this socket.
	 */
	virtual ~BufferedSocket();

	/**
	 * This method attempts to connect to a hostname.
	 * This method is asyncronous.
	 * @param maxtime Number of seconds to wait, if connecting, before the connection times out and an OnTimeout() event is generated
	 */
	virtual bool DoConnect(unsigned long maxtime);

	/** Handle event from EventHandler parent class
	 */
	void HandleEvent(EventType et, int errornum = 0);

	/** Returns true if this socket is readable
	 */
	bool Readable();
};

#endif
