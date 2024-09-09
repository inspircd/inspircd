/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

#include "timer.h"

class IOHook;

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
class CoreExport SocketTimeout final
	: public Timer
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
	 */
	SocketTimeout(int fd, BufferedSocket* thesock, unsigned long secs_from_now)
		: Timer(secs_from_now, false)
		, sock(thesock)
		, sfd(fd)
	{
	}

	/** Handle tick event
	 */
	bool Tick() override;
};

/**
 * StreamSocket is a class that wraps a TCP socket and handles send
 * and receive queues, including passing them to IO hooks
 */
class CoreExport StreamSocket
	: public EventHandler
{
public:
	/** Socket send queue
	 */
	class SendQueue final
	{
	public:
		/** One element of the queue, a continuous buffer
		 */
		typedef std::string Element;

		/** Sequence container of buffers in the queue
		 */
		typedef std::deque<Element> Container;

		/** Container iterator
		 */
		typedef Container::const_iterator const_iterator;

		/** Return whether the queue is empty
		 * @return True if the queue is empty, false otherwise
		 */
		bool empty() const { return (nbytes == 0); }

		/** Get the number of individual buffers in the queue
		 * @return Number of individual buffers in the queue
		 */
		Container::size_type size() const { return data.size(); }

		/** Get the number of queued bytes
		 * @return Size in bytes of the data in the queue
		 */
		size_t bytes() const { return nbytes; }

		/** Get the first buffer of the queue
		 * @return A reference to the first buffer in the queue
		 */
		const Element& front() const { return data.front(); }

		/** Get an iterator to the first buffer in the queue.
		 * The returned iterator cannot be used to make modifications to the queue,
		 * for that purpose the member functions push_*(), pop_front(), erase_front() and clear() can be used.
		 * @return Iterator referring to the first buffer in the queue, or end() if there are no elements.
		 */
		const_iterator begin() const { return data.begin(); }

		/** Get an iterator to the (theoretical) buffer one past the end of the queue.
		 * @return Iterator referring to one element past the end of the container
		 */
		const_iterator end() const { return data.end(); }

		/** Remove the first buffer in the queue
		 */
		void pop_front()
		{
			nbytes -= data.front().length();
			data.pop_front();
		}

		/** Remove bytes from the beginning of the first buffer
		 * @param n Number of bytes to remove
		 */
		void erase_front(Element::size_type n)
		{
			nbytes -= n;
			data.front().erase(0, n);
		}

		/** Insert a new buffer at the beginning of the queue
		 * @param newdata Data to add
		 */
		void push_front(const Element& newdata)
		{
			data.push_front(newdata);
			nbytes += newdata.length();
		}

		/** Insert a new buffer at the end of the queue
		 * @param newdata Data to add
		 */
		void push_back(const Element& newdata)
		{
			data.push_back(newdata);
			nbytes += newdata.length();
		}

		/** Clear the queue
		 */
		void clear()
		{
			data.clear();
			nbytes = 0;
		}

		void moveall(SendQueue& other)
		{
			nbytes += other.bytes();
			data.insert(data.end(), other.data.begin(), other.data.end());
			other.clear();
		}

	private:
		/** Private send queue. Note that individual strings may be shared.
		 */
		Container data;

		/** Length, in bytes, of the sendq
		 */
		size_t nbytes = 0;
	};

	/** The type of socket this IOHook represents. */
	enum Type
	{
		SS_UNKNOWN,
		SS_USER
	};

private:
	/** Whether this socket should close once its sendq is empty */
	bool closeonempty = false;

	/** Whether the socket is currently closing or not, used to avoid repeatedly closing a closed socket */
	bool closing = false;

	/** The IOHook that handles raw I/O for this socket, or NULL */
	IOHook* iohook = nullptr;

	/** Send queue of the socket
	 */
	SendQueue sendq;

	/** Error - if nonempty, the socket is dead, and this is the reason. */
	std::string error;

	/** Check if the socket has an error set, if yes, call OnError
	 * @param err Error to pass to OnError()
	 */
	void CheckError(BufferedSocketError err);

	/** Read data from the socket into the recvq, if successful call OnDataReady()
	 */
	void DoRead();

	/** Send as much data contained in a SendQueue object as possible.
	 * All data which successfully sent will be removed from the SendQueue.
	 * @param sq SendQueue to flush
	 */
	void FlushSendQ(SendQueue& sq);

	/** Read incoming data into a receive queue.
	 * @param rq Receive queue to put incoming data into
	 * @return < 0 on error or close, 0 if no new data is ready (but the socket is still connected), > 0 if data was read from the socket and put into the recvq
	 */
	ssize_t ReadToRecvQ(std::string& rq);

	/** Read data from a hook chain recursively, starting at 'hook'.
	 * If 'hook' is NULL, the recvq is filled with data from SocketEngine::Recv(), otherwise it is filled with data from the
	 * next hook in the chain.
	 * @param hook Next IOHook in the chain, can be NULL
	 * @param rq Receive queue to put incoming data into
	 * @return < 0 on error or close, 0 if no new data is ready (but the socket is still connected), > 0 if data was read from
	 * the socket and put into the recvq
	 */
	ssize_t HookChainRead(IOHook* hook, std::string& rq);

protected:
	/** The data which has been received from the socket. */
	std::string recvq;

public:
	const Type type;
	StreamSocket(Type sstype = SS_UNKNOWN)
		: type(sstype)
	{
	}
	IOHook* GetIOHook() const;
	void AddIOHook(IOHook* hook);
	void DelIOHook();

	/** Writes the contents of the send queue to the socket. */
	void DoWrite();

	/** Called by the socket engine when a read event happens. */
	void OnEventHandlerRead() override;

	/** Called by the socket engine when a write event happens. */
	void OnEventHandlerWrite() override;

	/** Called by the socket engine when an error happens.
	 * @param errcode The error code provided by the operating system.
	 */
	void OnEventHandlerError(int errcode) override;

	/** Sets the error message for this socket. Once set, the socket is dead. */
	void SetError(const std::string& err) { if (error.empty()) error = err; }

	/** Retrieves the error message for this socket. */
	const std::string& GetError() const { return error; }

	/** Called when new data is present in the receive queue. */
	virtual void OnDataReady() = 0;

	/** Called when the socket gets an error from socket engine or I/O hook. */
	virtual void OnError(BufferedSocketError e) = 0;

	/** Called when the local socket address address is changed.
	 * @param sa The new local socket address.
	 * @return true if the connection is still open, false if it has been closed
	 */
	virtual bool OnChangeLocalSocketAddress(const irc::sockets::sockaddrs& sa) { return true; }

	/** Called when the remote socket address address is changed.
	 * @param sa The new remote socket address.
	 * @return true if the connection is still open, false if it has been closed
	 */
	virtual bool OnChangeRemoteSocketAddress(const irc::sockets::sockaddrs& sa) { return true; }

	/** Send the given data out the socket, either now or when writes unblock
	 */
	void WriteData(const std::string& data);

	/** Retrieves the current size of the send queue. */
	size_t GetSendQSize() const;

	/** Retrieves the send queue. */
	SendQueue& GetSendQ() { return sendq; }

	/**
	 * Close the socket, remove from socket engine, etc
	 */
	virtual void Close();

	/** If writeblock is true then only close the socket if all data has been sent. Otherwise, close immediately. */
	void Close(bool writeblock);

	/** This ensures that close is called prior to destructor */
	Cullable::Result Cull() override;

	/** Get the IOHook of a module attached to this socket
	 * @param mod Module whose IOHook to return
	 * @return IOHook belonging to the module or NULL if the module haven't attached an IOHook to this socket
	 */
	IOHook* GetModHook(Module* mod) const;

	/** Get the last IOHook attached to this socket
	 * @return The last IOHook attached to this socket or NULL if no IOHooks are attached
	 */
	IOHook* GetLastHook() const;
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
class CoreExport BufferedSocket
	: public StreamSocket
{
public:
	/** Timeout object or NULL
	 */
	SocketTimeout* Timeout = nullptr;

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

	~BufferedSocket() override;

	/** Begin connection to the given address
	 * This will create a socket, register with socket engine, and start the asynchronous
	 * connection process. If an error is detected at this point (such as out of file descriptors),
	 * OnError will be called; otherwise, the state will become CONNECTING.
	 * @param dest Remote endpoint to connect to.
	 * @param bind Local endpoint to connect from.
	 * @param maxtime Time to wait for connection
	 * @param protocol The protocol to use when connecting.
	 */
	void DoConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long maxtime, sa_family_t protocol = AF_UNSPEC);

	/** This method is called when an outbound connection on your socket is
	 * completed.
	 */
	virtual void OnConnected();

	/** When there is data waiting to be read on a socket, the OnDataReady()
	 * method is called.
	 */
	void OnDataReady() override = 0;

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

protected:
	void OnEventHandlerWrite() override;
	BufferedSocketError BeginConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long timeout, sa_family_t protocol = AF_UNSPEC);
};

inline IOHook* StreamSocket::GetIOHook() const { return iohook; }
inline void StreamSocket::DelIOHook() { iohook = nullptr; }
