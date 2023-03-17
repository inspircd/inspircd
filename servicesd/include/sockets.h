/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#ifndef SOCKETS_H
#define SOCKETS_H

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include "anope.h"

#define NET_BUFSIZE 65535

/** A sockaddr union used to combine IPv4 and IPv6 sockaddrs
 */
union CoreExport sockaddrs {
    sockaddr sa;
    sockaddr_in sa4;
    sockaddr_in6 sa6;

    /** Construct the object, sets everything to 0
     */
    sockaddrs(const Anope::string &address = "");

    /** Memset the object to 0
     */
    void clear();

    /** Get address family.
     */
    int family() const;

    /** Get the size of the sockaddr we represent
     * @return The size
     */
    size_t size() const;

    /** Get the port represented by this addr
     * @return The port, or -1 on fail
     */
    int port() const;

    /** Get the address represented by this addr
     * @return The address
     */
    Anope::string addr() const;

    /** Get the reverse address represented by this addr
     * @return The reverse address
     */
    Anope::string reverse() const;

    /* Is this address ipv6? */
    bool ipv6() const;

    /** Check if this sockaddr has data in it
     */
    bool valid() const;

    /** Compares with sockaddr with another. Compares address type, port, and address
     * @return true if they are the same
     */
    bool operator==(const sockaddrs &other) const;
    /* The same as above but not */
    inline bool operator!=(const sockaddrs &other) const {
        return !(*this == other);
    }

    /** The equivalent of inet_pton
     * @param type AF_INET or AF_INET6
     * @param address The address to place in the sockaddr structures
     * @param pport An option port to include in the  sockaddr structures
     * @throws A socket exception if given invalid IPs
     */
    void pton(int type, const Anope::string &address, int pport = 0);

    /** The equivalent of inet_ntop
     * @param type AF_INET or AF_INET6
     * @param address The in_addr or in_addr6 structure
     * @throws A socket exception if given an invalid structure
     */
    void ntop(int type, const void *src);
};

class CoreExport cidr {
    sockaddrs addr;
    Anope::string cidr_ip;
    unsigned short cidr_len;
  public:
    cidr(const Anope::string &ip);
    cidr(const Anope::string &ip, unsigned char len);
    cidr(const sockaddrs &ip, unsigned char len);
    Anope::string mask() const;
    bool match(const sockaddrs &other);
    bool valid() const;

    bool operator<(const cidr &other) const;
    bool operator==(const cidr &other) const;
    bool operator!=(const cidr &other) const;

    struct CoreExport hash {
        size_t operator()(const cidr &s) const;
    };
};

class SocketException : public CoreException {
  public:
    /** Constructor for socket exceptions
     * @param message Error message
     */
    SocketException(const Anope::string &message) : CoreException(message) { }

    /** Destructor
     * @throws Nothing
     */
    virtual ~SocketException() throw() { }
};

enum SocketFlag {
    SF_DEAD = 1,
    SF_READABLE,
    SF_WRITABLE,
    SF_CONNECTING,
    SF_CONNECTED,
    SF_ACCEPTING,
    SF_ACCEPTED,
    SF_SIZE
};

class CoreExport SocketIO {
  public:
    virtual ~SocketIO() { }

    /** Receive something from the buffer
     * @param s The socket
     * @param buf The buf to read to
     * @param sz How much to read
     * @return Number of bytes received
     */
    virtual int Recv(Socket *s, char *buf, size_t sz);

    /** Write something to the socket
     * @param s The socket
     * @param buf The data to write
     * @param size The length of the data
     */
    virtual int Send(Socket *s, const char *buf, size_t sz);
    int Send(Socket *s, const Anope::string &buf);

    /** Accept a connection from a socket
     * @param s The socket
     * @return The new socket
     */
    virtual ClientSocket *Accept(ListenSocket *s);

    /** Finished accepting a connection from a socket
     * @param s The socket
     * @return SF_ACCEPTED if accepted, SF_ACCEPTING if still in process, SF_DEAD on error
     */
    virtual SocketFlag FinishAccept(ClientSocket *cs);

    /** Bind a socket
     * @param s The socket
     * @param ip The IP to bind to
     * @param port The optional port to bind to
     */
    virtual void Bind(Socket *s, const Anope::string &ip, int port = 0);

    /** Connect the socket
     * @param s The socket
     * @param target IP to connect to
     * @param port to connect to
     */
    virtual void Connect(ConnectionSocket *s, const Anope::string &target,
                         int port);

    /** Called to potentially finish a pending connection
     * @param s The socket
     * @return SF_CONNECTED on success, SF_CONNECTING if still pending, and SF_DEAD on error.
     */
    virtual SocketFlag FinishConnect(ConnectionSocket *s);

    /** Called when the socket is destructing
     */
    virtual void Destroy() { }
};

class CoreExport Socket {
  protected:
    /* Socket FD */
    int sock;
    /* Is this an IPv6 socket? */
    bool ipv6;

  public:
    std::bitset<SF_SIZE> flags;

    /* Sockaddrs for bind() (if it's bound) */
    sockaddrs bindaddr;

    /* I/O functions used for this socket */
    SocketIO *io;

    /** Empty constructor, should not be called.
     */
    Socket();

    /** Constructor, possibly creates the socket and adds it to the engine
     * @param sock The socket to use, -1 if we need to create our own
     * @param ipv6 true if using ipv6
     * @param type The socket type, defaults to SOCK_STREAM
     */
    Socket(int sock, bool ipv6 = false, int type = SOCK_STREAM);

    /** Destructor, closes the socket and removes it from the engine
     */
    virtual ~Socket();

    /** Get the socket FD for this socket
     * @return the fd
     */
    int GetFD() const;

    /** Check if this socket is IPv6
     * @return true or false
     */
    bool IsIPv6() const;

    /** Mark a socket as (non)blocking
     * @param state true to enable blocking, false to disable blocking
     * @return true if the socket is now blocking
     */
    bool SetBlocking(bool state);

    /** Bind the socket to an ip and port
     * @param ip The ip
     * @param port The port
     */
    void Bind(const Anope::string &ip, int port = 0);

    /** Called when there either is a read or write event.
     * @return true to continue to call ProcessRead/ProcessWrite, false to not continue
     */
    virtual bool Process();

    /** Called when there is something to be received for this socket
     * @return true on success, false to drop this socket
     */
    virtual bool ProcessRead();

    /** Called when the socket is ready to be written to
     * @return true on success, false to drop this socket
     */
    virtual bool ProcessWrite();

    /** Called when there is an error for this socket
     * @return true on success, false to drop this socket
     */
    virtual void ProcessError();
};

class CoreExport BufferedSocket : public virtual Socket {
  protected:
    /* Things read from the socket */
    Anope::string read_buffer;
    /* Things to be written to the socket */
    Anope::string write_buffer;
    /* How much data was received from this socket on this recv() */
    int recv_len;

  public:
    BufferedSocket();
    virtual ~BufferedSocket();

    /** Called when there is something to be received for this socket
     * @return true on success, false to drop this socket
     */
    bool ProcessRead() anope_override;

    /** Called when the socket is ready to be written to
     * @return true on success, false to drop this socket
     */
    bool ProcessWrite() anope_override;

    /** Gets the new line from the input buffer, if any
     */
    const Anope::string GetLine();

    /** Write to the socket
    * @param message The message
    */
  protected:
    virtual void Write(const char *buffer, size_t l);
  public:
    void Write(const char *message, ...);
    void Write(const Anope::string &message);

    /** Get the length of the read buffer
     * @return The length of the read buffer
     */
    int ReadBufferLen() const;

    /** Get the length of the write buffer
     * @return The length of the write buffer
     */
    int WriteBufferLen() const;
};

class CoreExport BinarySocket : public virtual Socket {
  protected:
    struct DataBlock {
        char *orig;
        char *buf;
        size_t len;

        DataBlock(const char *b, size_t l);
        ~DataBlock();
    };

    /* Data to be written out */
    std::deque<DataBlock *> write_buffer;

  public:
    BinarySocket();
    virtual ~BinarySocket();

    /** Called when there is something to be received for this socket
     * @return true on success, false to drop this socket
     */
    bool ProcessRead() anope_override;

    /** Called when the socket is ready to be written to
     * @return true on success, false to drop this socket
     */
    bool ProcessWrite() anope_override;

    /** Write data to the socket
     * @param buffer The data to write
     * @param l The length of the data; if 0 then this function returns without doing anything
     */
    virtual void Write(const char *buffer, size_t l);
    void Write(const char *message, ...);
    void Write(const Anope::string &message);

    /** Called with data from the socket
     * @param buffer The data
     * @param l The length of buffer
     * @return true to continue reading, false to drop the socket
     */
    virtual bool Read(const char *buffer, size_t l);
};

class CoreExport ListenSocket : public virtual Socket {
  public:
    /** Constructor
     * @param bindip The IP to bind to
     * @param port The port to listen on
     * @param ipv6 true for ipv6
     */
    ListenSocket(const Anope::string &bindip, int port, bool ipv6);
    virtual ~ListenSocket();

    /** Process what has come in from the connection
     * @return false to destroy this socket
     */
    bool ProcessRead();

    /** Called when a connection is accepted
     * @param fd The FD for the new connection
     * @param addr The sockaddr for where the connection came from
     * @return The new socket
     */
    virtual ClientSocket *OnAccept(int fd, const sockaddrs &addr) = 0;
};

class CoreExport ConnectionSocket : public virtual Socket {
  public:
    /* Sockaddrs for connection ip/port */
    sockaddrs conaddr;

    /** Connect the socket
     * @param TargetHost The target host to connect to
     * @param Port The target port to connect to
     */
    void Connect(const Anope::string &TargetHost, int Port);

    /** Called when there either is a read or write event.
     * Used to determine whether or not this socket is connected yet.
     * @return true to continue to call ProcessRead/ProcessWrite, false to not continue
     */
    bool Process() anope_override;

    /** Called when there is an error for this socket
     * @return true on success, false to drop this socket
     */
    void ProcessError() anope_override;

    /** Called on a successful connect
     */
    virtual void OnConnect();

    /** Called when a connection is not successful
     * @param error The error
     */
    virtual void OnError(const Anope::string &error);
};

class CoreExport ClientSocket : public virtual Socket {
  public:
    /* Listen socket this connection came from */
    ListenSocket *ls;
    /* Clients address */
    sockaddrs clientaddr;

    /** Constructor
     * @param ls Listen socket this connection is from
     * @param addr Address the connection came from
     */
    ClientSocket(ListenSocket *ls, const sockaddrs &addr);

    /** Called when there either is a read or write event.
     * Used to determine whether or not this socket is connected yet.
     * @return true to continue to call ProcessRead/ProcessWrite, false to not continue
     */
    bool Process() anope_override;

    /** Called when there is an error for this socket
     * @return true on success, false to drop this socket
     */
    void ProcessError() anope_override;

    /** Called when a client has been accepted() successfully.
     */
    virtual void OnAccept();

    /** Called when there was an error accepting the client
     */
    virtual void OnError(const Anope::string &error);
};

class CoreExport Pipe : public Socket {
  public:
    /** The FD of the write pipe
     * this->sock is the readfd
     */
    int write_pipe;

    Pipe();
    ~Pipe();

    /** Called when data is to be read, reads the data then calls OnNotify
     */
    bool ProcessRead() anope_override;

    /** Write data to this pipe
     * @param data The data to write
     * @param sz The amount of data to wirite
     */
    void Write(const char *data, size_t sz);
    inline void Write(const Anope::string &data) {
        this->Write(data.c_str(), data.length() + 1);
    }

    /** Read data from this pipe
     * @param data A buffer to read data into
     * @param sz The size of the buffer
     * @return The amount of data read
     */
    int Read(char *data, size_t sz);

    /** Mark the write end of this pipe (non)blocking
     * @param state true to enable blocking, false to disable blocking
     * @return true if the socket is now blocking
     */
    bool SetWriteBlocking(bool state);

    /** Called when this pipe needs to be woken up
     * Is the same as Write("\0", 1)
     */
    void Notify();

    /** Called after ProcessRead comes back from Notify(), overload to do something useful
     */
    virtual void OnNotify() = 0;
};

extern CoreExport uint32_t TotalRead;
extern CoreExport uint32_t TotalWritten;
extern CoreExport SocketIO NormalSocketIO;

#endif // SOCKET_H
