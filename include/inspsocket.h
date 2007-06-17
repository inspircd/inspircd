/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __INSP_SOCKET_H__
#define __INSP_SOCKET_H__

#include <sstream>
#include <string>
#include <deque>
#include "dns.h"
#include "inspircd_config.h"
#include "socket.h"
#include "inspsocket.h"
#include "timer.h"

/**
 * States which a socket may be in
 */
enum InspSocketState
{
	/** Socket disconnected */
	I_DISCONNECTED,
	/** Socket connecting */
	I_CONNECTING,
	/** Socket fully connected */
	I_CONNECTED,
	/** Socket listening for connections */
	I_LISTENING,
	/** Socket has an error */
	I_ERROR
};

/**
 * Error types which a socket may exhibit
 */
enum InspSocketError
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
class InspSocket;
class InspIRCd;

using irc::sockets::insp_sockaddr;
using irc::sockets::insp_inaddr;
using irc::sockets::insp_ntoa;
using irc::sockets::insp_aton;

/** Used to time out socket connections
 */
class CoreExport SocketTimeout : public InspTimer
{
 private:
	/** InspSocket the class is attached to
	 */
	InspSocket* sock;
	/** Server instance creating the timeout class
	 */
	InspIRCd* ServerInstance;
	/** File descriptor of class this is attached to
	 */
	int sfd;
 public:
	/** Create a socket timeout class
	 * @param fd File descriptor of InspSocket
	 * @pram Instance server instance to attach to
	 * @param thesock InspSocket to attach to
	 * @param secs_from_now Seconds from now to time out
	 * @param now The current time
	 */
	SocketTimeout(int fd, InspIRCd* Instance, InspSocket* thesock, long secs_from_now, time_t now) : InspTimer(secs_from_now, now), sock(thesock), ServerInstance(Instance), sfd(fd) { };
	/** Handle tick event
	 */
	virtual void Tick(time_t now);
};

/**
 * InspSocket is an extendable socket class which modules
 * can use for TCP socket support. It is fully integrated
 * into InspIRCds socket loop and attaches its sockets to
 * the core's instance of the SocketEngine class, meaning
 * that any sockets you create have the same power and
 * abilities as a socket created by the core itself.
 * To use InspSocket, you must inherit a class from it,
 * and use the InspSocket constructors to establish connections
 * and bindings.
 */
class CoreExport InspSocket : public EventHandler
{
 public:

	/** 
	 * Bind IP
	 */
	std::string cbindip;

	/** 
	 * Is hooked by a module for low level IO
	 */
	bool IsIOHooked;

	/** 
	 * Instance we were created by
	 */
	InspIRCd* Instance;

	/** 
	 * Timeout class or NULL
	 */
	SocketTimeout* Timeout;

	/** 
	 * Timeout length
	 */
	unsigned long timeout_val;

	/** 
	 * Socket output buffer (binary safe)
	 */
	std::deque<std::string> outbuffer;

	/**
	 * The hostname connected to
	 */
	char host[MAXBUF];

	/**
	 * The port connected to, or the port
	 * this socket is listening on
	 */
	int port;

	/**
	 * The state for this socket, either
	 * listening, connecting, connected
	 * or error.
	 */
	InspSocketState state;

	/**
	 * This value is true if the
	 * socket has timed out.
	 */
	bool timeout;
	
	/**
	 * Socket input buffer, used by read(). The class which
	 * extends InspSocket is expected to implement an extendable
	 * buffer which can grow much larger than 64k,
	 * this buffer is just designed to be temporary storage.
	 * space.
	 */
	char ibuf[65535];

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
	 */
	bool FlushWriteBuffer();

	/** Set the queue sizes
	 * This private method sets the operating system queue
	 * sizes for this socket to 65535 so that it can queue
	 * more information without application-level queueing
	 * which was required in older software.
	 */
	void SetQueues(int nfd);

	/** When the socket has been marked as closing, this flag
	 * will be set to true, then the next time the socket is
	 * examined, the socket is deleted and closed.
	 */
	bool ClosePending;

	/** Set to true when we're waiting for a write event.
	 * If this is true and a write event comes in, we
	 * call the write instead of the read method.
	 */
	bool WaitingForWriteEvent;

	/**
	 * Bind to an address
	 * @param ip IP to bind to
	 * @return True is the binding succeeded
	 */
	bool BindAddr(const std::string &ip);

	/**
	 * The default constructor does nothing
	 * and should not be used.
	 */
	InspSocket(InspIRCd* SI);

	/**
	 * This constructor is used to associate
	 * an existing connecting with an InspSocket
	 * class. The given file descriptor must be
	 * valid, and when initialized, the InspSocket
	 * will be set with the given IP address
	 * and placed in CONNECTED state.
	 */
	InspSocket(InspIRCd* SI, int newfd, const char* ip);

	/**
	 * This constructor is used to create a new
	 * socket, either listening for connections, or an outbound connection to another host.
	 * Note that if you specify a hostname in the 'ipaddr' parameter, this class will not
	 * connect. You must resolve your hostnames before passing them to InspSocket. To do so,
	 * you should use the nonblocking class 'Resolver'.
	 * @param ipaddr The IP to connect to, or bind to
	 * @param port The port number to connect to, or bind to
	 * @param listening true to listen on the given host:port pair, or false to connect to them
	 * @param maxtime Number of seconds to wait, if connecting, before the connection times out and an OnTimeout() event is generated
	 * @param connectbindip When creating an outbound connection, the IP to bind the connection to. If not defined, the port is not bound.
	 * @return On exit, GetState() returns I_ERROR if an error occured, and errno can be used to read the socket error.
	 */
	InspSocket(InspIRCd* SI, const std::string &ipaddr, int port, bool listening, unsigned long maxtime, const std::string &connectbindip = "");

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
	virtual void OnError(InspSocketError e);

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
	 * calls InspSocket::Close() and deletes it.
	 * @return false to close the socket
	 */
	virtual bool OnDataReady();

	/**
	 * When it is ok to write to the socket, and a 
	 * write event was requested, this method is
	 * triggered. Within this method you should call
	 * write() or send() etc, to send data to the
	 * other end of the socket. Further write events
	 * will not be triggered unless you call WantWrite().
	 * @return false to close the socket
	 */
	virtual bool OnWriteReady();

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
	 * OnTimeout(), and Close(), and also when
	 * cancelling a listening socket by calling
	 * the destructor indirectly.
	 */
	virtual void OnClose();

	/**
	 * Reads all pending bytes from the socket
	 * into a char* array which can be up to
	 * 16 kilobytes in length.
	 */
	virtual char* Read();

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
	virtual int Write(const std::string &data);

	/**
	 * If your socket is a listening socket, when a new
	 * connection comes in on the socket this method will
	 * be called. Given the new file descriptor in the
	 * parameters, and the IP, it is recommended you copy
	 * them to a new instance of your socket class,
	 * e.g.:
	 *
	 * MySocket* newsocket = new MySocket(newfd,ip);
	 *
	 * Once you have done this, you can then associate the
	 * new socket with the core using Server::AddSocket().
	 */
	virtual int OnIncomingConnection(int newfd, char* ip);

	/**
	 * Changes the socket's state. The core uses this
	 * to change socket states, and you should not call
	 * it directly.
	 */
	void SetState(InspSocketState s);

	/**
	 * Call this to receive the next write event
	 * that comes along for this fd to the OnWriteReady
	 * method.
	 */
	void WantWrite();

	/**
	 * Returns the current socket state.
	 */
	InspSocketState GetState();

	/**
	 * Only the core should call this function.
	 * When called, it is assumed the socket is ready
	 * to read data, and the method call routes the
	 * event to the various methods of InspSocket
	 * for you to handle. This can also cause the
	 * socket's state to change.
	 */
	bool Poll();

	/**
	 * This method returns the socket's file descriptor
	 * as assigned by the operating system, or -1
	 * if no descriptor has been assigned.
	 */
	int GetFd();

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
	virtual ~InspSocket();

	/**
	 * This method attempts to connect to a hostname.
	 * This only occurs on a non-listening socket. This
	 * method is asyncronous.
	 */
	virtual bool DoConnect();

	/**
	 * This method marks the socket closed.
	 * The next time the core examines a socket marked
	 * as closed, the socket will be closed and the 
	 * memory reclaimed.
	 *
	 * NOTE: This method is DEPRECIATED and will be
	 * ignored if called!
	 */
	void MarkAsClosed();

	/** Handle event from EventHandler parent class
	 */
	void HandleEvent(EventType et, int errornum = 0);

	/** Returns true if this socket is readable
	 */
	bool Readable();
};

#endif

