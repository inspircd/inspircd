/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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


#ifndef INSPSOCKET_H
#define INSPSOCKET_H

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
	/** No error */
	I_ERR_NONE,
	/** Socket was closed by peer */
	I_ERR_DISCONNECT,
	/** Socket connect timed out */
	I_ERR_TIMEOUT,
	/** Socket could not be created */
	I_ERR_SOCKET,
	/** Socket could not connect (refused) */
	I_ERR_CONNECT,
	/** Socket could not bind to local port/ip */
	I_ERR_BIND,
	/** Socket could not write data */
	I_ERR_WRITE,
	/** No more file descriptors left to create socket! */
	I_ERR_NOMOREFDS,
	/** Some other error */
	I_ERR_OTHER
};

/* Required forward declarations */
class BufferedSocket;

/** Used to time out socket connections
 */
class CoreExport SocketTimeout : public Timer
{
 private:
	/** BufferedSocket the class is attached to
	 */
	BufferedSocket* sock;

	/** File descriptor of class this is attached to
	 */
	int sfd;

 public:
	/** Create a socket timeout class
	 * @param fd File descriptor of BufferedSocket
	 * @param thesock BufferedSocket to attach to
	 * @param secs_from_now Seconds from now to time out
	 * @param now The current time
	 */
	SocketTimeout(int fd, BufferedSocket* thesock, long secs_from_now, time_t now) : Timer(secs_from_now, now), sock(thesock), sfd(fd) { }

	/** Handle tick event
	 */
	virtual void Tick(time_t now);
};

/**
 * StreamSocket is a class that wraps a TCP socket and handles send
 * and receive queues, including passing them to IO hooks
 */
class CoreExport StreamSocket : public EventHandler
{
	/** Module that handles raw I/O for this socket, or NULL */
	reference<Module> IOHook;
	/** Private send queue. Note that individual strings may be shared
	 */
	std::deque<std::string> sendq;
	/** Length, in bytes, of the sendq */
	size_t sendq_len;
	/** Error - if nonempty, the socket is dead, and this is the reason. */
	std::string error;
 protected:
	std::string recvq;
 public:
	StreamSocket() : sendq_len(0) {}
	inline Module* GetIOHook();
	inline void AddIOHook(Module* m);
	inline void DelIOHook();
	/** Handle event from socket engine.
	 * This will call OnDataReady if there is *new* data in recvq
	 */
	virtual void HandleEvent(EventType et, int errornum = 0);
	/** Dispatched from HandleEvent */
	virtual void DoRead();
	/** Dispatched from HandleEvent */
	virtual void DoWrite();

	/** Sets the error message for this socket. Once set, the socket is dead. */
	void SetError(const std::string& err) { if (error.empty()) error = err; }

	/** Gets the error message for this socket. */
	const std::string& getError() const { return error; }

	/** Called when new data is present in recvq */
	virtual void OnDataReady() = 0;
	/** Called when the socket gets an error from socket engine or IO hook */
	virtual void OnError(BufferedSocketError e) = 0;

	/** Send the given data out the socket, either now or when writes unblock
	 */
	void WriteData(const std::string& data);
	/** Convenience function: read a line from the socket
	 * @param line The line read
	 * @param delim The line delimiter
	 * @return true if a line was read
	 */
	bool GetNextLine(std::string& line, char delim = '\n');
	/** Useful for implementing sendq exceeded */
	inline size_t getSendQSize() const { return sendq_len; }

	/**
	 * Close the socket, remove from socket engine, etc
	 */
	virtual void Close();
	/** This ensures that close is called prior to destructor */
	virtual CullResult cull();
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
class CoreExport BufferedSocket : public StreamSocket
{
 public:
	/** Timeout object or NULL
	 */
	SocketTimeout* Timeout;

	/**
	 * The state for this socket, either
	 * listening, connecting, connected
	 * or error.
	 */
	BufferedSocketState state;

	BufferedSocket();
	/**
	 * This constructor is used to associate
	 * an existing connecting with an BufferedSocket
	 * class. The given file descriptor must be
	 * valid, and when initialized, the BufferedSocket
	 * will be placed in CONNECTED state.
	 */
	BufferedSocket(int newfd);

	/** Begin connection to the given address
	 * This will create a socket, register with socket engine, and start the asynchronous
	 * connection process. If an error is detected at this point (such as out of file descriptors),
	 * OnError will be called; otherwise, the state will become CONNECTING.
	 * @param ipaddr Address to connect to
	 * @param aport Port to connect on
	 * @param maxtime Time to wait for connection
	 * @param connectbindip Address to bind to (if NULL, no bind will be done)
	 */
	void DoConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip);

	/** This method is called when an outbound connection on your socket is
	 * completed.
	 */
	virtual void OnConnected();

	/** When there is data waiting to be read on a socket, the OnDataReady()
	 * method is called.
	 */
	virtual void OnDataReady() = 0;

	/**
	 * When an outbound connection fails, and the attempt times out, you
	 * will receive this event.  The method will trigger once maxtime
	 * seconds are reached (as given in the constructor) just before the
	 * socket's descriptor is closed.  A failed DNS lookup may cause this
	 * event if the DNS server is not responding, as well as a failed
	 * connect() call, because DNS lookups are nonblocking as implemented by
	 * this class.
	 */
	virtual void OnTimeout();

	virtual ~BufferedSocket();
 protected:
	virtual void DoWrite();
	BufferedSocketError BeginConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long timeout);
	BufferedSocketError BeginConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip);
};

#include "modules.h"

inline Module* StreamSocket::GetIOHook() { return IOHook; }
inline void StreamSocket::AddIOHook(Module* m) { IOHook = m; }
inline void StreamSocket::DelIOHook() { IOHook = NULL; }
#endif
