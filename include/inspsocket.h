/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
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
enum InspSocketState { I_DISCONNECTED, I_CONNECTING, I_CONNECTED, I_LISTENING, I_ERROR };

/**
 * Error types which a socket may exhibit
 */
enum InspSocketError { I_ERR_TIMEOUT, I_ERR_SOCKET, I_ERR_CONNECT, I_ERR_BIND, I_ERR_RESOLVE, I_ERR_WRITE, I_ERR_NOMOREFDS };

class InspSocket;
class InspIRCd;

using irc::sockets::insp_sockaddr;
using irc::sockets::insp_inaddr;
using irc::sockets::insp_ntoa;
using irc::sockets::insp_aton;

/** Used to time out socket connections
 */
class SocketTimeout : public InspTimer
{
 private:
	InspSocket* sock;
	InspIRCd* ServerInstance;
	int sfd;
 public:
	SocketTimeout(int fd, InspIRCd* Instance, InspSocket* thesock, long secs_from_now, time_t now) : InspTimer(secs_from_now, now), sock(thesock), ServerInstance(Instance), sfd(fd) { };
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
class InspSocket : public EventHandler
{
 public:
	InspIRCd* Instance;

	SocketTimeout* Timeout;

	unsigned long timeout_val;

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
	 * The host being connected to,
	 * in sockaddr form
	 */
        insp_sockaddr addr;

	/** 
	 * The host being connected to,
	 * in in_addr form
	 */
        insp_inaddr addy;

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
	 * Client sockaddr structure used
	 * by accept()
	 */
	insp_sockaddr client;

	/**
	 * Server sockaddr structure used
	 * by accept()
	 */
	insp_sockaddr server;

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

	bool BindAddr();

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
	 */
	InspSocket(InspIRCd* SI, const std::string &ipaddr, int port, bool listening, unsigned long maxtime);

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
	 */
	void MarkAsClosed();

	void HandleEvent(EventType et, int errornum = 0);

	bool Readable();
};

#endif
