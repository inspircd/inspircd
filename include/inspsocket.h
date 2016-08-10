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
	SocketTimeout(int fd, BufferedSocket* thesock, long secs_from_now) : Timer(secs_from_now), sock(thesock), sfd(fd) { }

	/** Handle tick event
	 */
	virtual bool Tick(time_t now);
};

/**
 * StreamSocket is a class that wraps a TCP socket and handles send
 * and receive queues, including passing them to IO hooks
 */
class CoreExport StreamSocket : public EventHandler
{
 public:
	/** Socket send queue
	 */
	class SendQueue
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

		SendQueue() : nbytes(0) { }

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
		size_t nbytes;
	};

 private:
	/** The IOHook that handles raw I/O for this socket, or NULL */
	IOHook* iohook;

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
	int ReadToRecvQ(std::string& rq);

	/** Read data from a hook chain recursively, starting at 'hook'.
	 * If 'hook' is NULL, the recvq is filled with data from SocketEngine::Recv(), otherwise it is filled with data from the
	 * next hook in the chain.
	 * @param hook Next IOHook in the chain, can be NULL
	 * @param rq Receive queue to put incoming data into
	 * @return < 0 on error or close, 0 if no new data is ready (but the socket is still connected), > 0 if data was read from
	 the socket and put into the recvq
	 */
	int HookChainRead(IOHook* hook, std::string& rq);

 protected:
	std::string recvq;
 public:
	StreamSocket() : iohook(NULL) { }
	IOHook* GetIOHook() const;
	void AddIOHook(IOHook* hook);
	void DelIOHook();

	/** Flush the send queue
	 */
	void DoWrite();

	/** Called by the socket engine on a read event
	 */
	void OnEventHandlerRead() CXX11_OVERRIDE;

	/** Called by the socket engine on a write event
	 */
	void OnEventHandlerWrite() CXX11_OVERRIDE;

	/** Called by the socket engine on error
	 * @param errcode Error
	 */
	void OnEventHandlerError(int errcode) CXX11_OVERRIDE;

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
	size_t getSendQSize() const;

	SendQueue& GetSendQ() { return sendq; }

	/**
	 * Close the socket, remove from socket engine, etc
	 */
	virtual void Close();
	/** This ensures that close is called prior to destructor */
	virtual CullResult cull();

	/** Get the IOHook of a module attached to this socket
	 * @param mod Module whose IOHook to return
	 * @return IOHook belonging to the module or NULL if the module haven't attached an IOHook to this socket
	 */
	IOHook* GetModHook(Module* mod) const;
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
	void OnEventHandlerWrite() CXX11_OVERRIDE;
	BufferedSocketError BeginConnect(const irc::sockets::sockaddrs& dest, const irc::sockets::sockaddrs& bind, unsigned long timeout);
	BufferedSocketError BeginConnect(const std::string &ipaddr, int aport, unsigned long maxtime, const std::string &connectbindip);
};

inline IOHook* StreamSocket::GetIOHook() const { return iohook; }
inline void StreamSocket::DelIOHook() { iohook = NULL; }
