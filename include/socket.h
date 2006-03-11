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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include "dns.h"

/**
 * States which a socket may be in
 */
enum InspSocketState { I_DISCONNECTED, I_RESOLVING, I_CONNECTING, I_CONNECTED, I_LISTENING, I_ERROR };

/**
 * Error types which a socket may exhibit
 */
enum InspSocketError { I_ERR_TIMEOUT, I_ERR_SOCKET, I_ERR_CONNECT, I_ERR_BIND, I_ERR_RESOLVE, I_ERR_WRITE };

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
class InspSocket
{
private:

	/**
	 * The file descriptor of this socket
	 */
        int fd;

	/**
	 * The resolver for this socket
	 */
	DNS dns;

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
        sockaddr_in addr;

	/** 
	 * The host being connected to,
	 * in in_addr form
	 */
        in_addr addy;

	/**
	 * When this time is reached,
	 * the socket times out if it is
	 * in the CONNECTING state
	 */
        time_t timeout_end;

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
	sockaddr_in client;

	/**
	 * Server sockaddr structure used
	 * by accept()
	 */
	sockaddr_in server;

	/**
	 * Used by accept() to indicate the
	 * sizes of the sockaddr_in structures
	 */
	socklen_t length;

	/** Flushes the write buffer
	 */
	bool FlushWriteBuffer();

	void SetQueues(int nfd);

	bool ClosePending;

public:

	/**
	 * The default constructor does nothing
	 * and should not be used.
	 */
	InspSocket();

	/**
	 * This constructor is used to associate
	 * an existing connecting with an InspSocket
	 * class. The given file descriptor must be
	 * valid, and when initialized, the InspSocket
	 * will be set with the given IP address
	 * and placed in CONNECTED state.
	 */
	InspSocket(int newfd, char* ip);

	/**
	 * This constructor is used to create a new
	 * socket, either listening for connections, or an outbound connection to another host.
	 * Note that if you specify a hostname in the 'host' parameter, then there will be an extra
	 * step involved (a nonblocking DNS lookup) which will cause your connection to be established
	 * slower than if it was an IP. Therefore, use an IP address where it is available instead.
	 * @param host The hostname to connect to, or bind to
	 * @param port The port number to connect to, or bind to
	 * @param listening true to listen on the given host:port pair, or false to connect to them
	 * @param maxtime Number of seconds to wait, if connecting, before the connection times out and an OnTimeout() event is generated
	 */
	InspSocket(const std::string &host, int port, bool listening, unsigned long maxtime);

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
	 * This function checks if the socket has
	 * timed out yet, given the current time
	 * in the parameter.
	 * @return true if timed out, false if not timed out
	 */
	bool Timeout(time_t current);

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

	virtual bool DoResolve();
	virtual bool DoConnect();

	void MarkAsClosed();
};

#endif
