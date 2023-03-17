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

#include "services.h"
#include "sockets.h"
#include "socketengine.h"
#include "logger.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#endif

std::map<int, Socket *> SocketEngine::Sockets;

uint32_t TotalRead = 0;
uint32_t TotalWritten = 0;

SocketIO NormalSocketIO;

sockaddrs::sockaddrs(const Anope::string &address) {
    this->clear();
    if (!address.empty()
            && address.find_first_not_of_ci("0123456789abcdef.:") == Anope::string::npos) {
        this->pton(address.find(':') != Anope::string::npos ? AF_INET6 : AF_INET,
                   address);
    }
}

void sockaddrs::clear() {
    memset(this, 0, sizeof(*this));
}

int sockaddrs::family() const {
    return sa.sa_family;
}

size_t sockaddrs::size() const {
    switch (sa.sa_family) {
    case AF_INET:
        return sizeof(sa4);
    case AF_INET6:
        return sizeof(sa6);
    default:
        break;
    }

    return 0;
}

int sockaddrs::port() const {
    switch (sa.sa_family) {
    case AF_INET:
        return ntohs(sa4.sin_port);
    case AF_INET6:
        return ntohs(sa6.sin6_port);
    default:
        break;
    }

    return -1;
}

Anope::string sockaddrs::addr() const {
    char address[INET6_ADDRSTRLEN];

    switch (sa.sa_family) {
    case AF_INET:
        if (inet_ntop(AF_INET, &sa4.sin_addr, address, sizeof(address))) {
            return address;
        }
        break;
    case AF_INET6:
        if (inet_ntop(AF_INET6, &sa6.sin6_addr, address, sizeof(address))) {
            return address;
        }
        break;
    default:
        break;
    }

    return "";
}

Anope::string sockaddrs::reverse() const {
    char address[128];

    switch (sa.sa_family) {
    case AF_INET6: {
        static const char hex[] = "0123456789abcdef";
        unsigned reverse_ip_count = 0;
        for (int j = 15; j >= 0; --j) {
            address[reverse_ip_count++] = hex[sa6.sin6_addr.s6_addr[j] & 0xF];
            address[reverse_ip_count++] = '.';
            address[reverse_ip_count++] = hex[sa6.sin6_addr.s6_addr[j] >> 4];
            address[reverse_ip_count++] = '.';
        }
        /* Remove the last '.' */
        address[reverse_ip_count - 1] = 0;
        return address;
    }
    case AF_INET: {
        unsigned long forward = sa4.sin_addr.s_addr;
        in_addr rev;
        rev.s_addr = forward << 24 | (forward & 0xFF00) << 8 | (forward & 0xFF0000) >> 8
                     | forward >> 24;
        if (inet_ntop(AF_INET, &rev, address, sizeof(address))) {
            return address;
        }
        break;
    }
    }

    return "";
}

bool sockaddrs::ipv6() const {
    return sa.sa_family == AF_INET6;
}

bool sockaddrs::valid() const {
    return size() != 0;
}

bool sockaddrs::operator==(const sockaddrs &other) const {
    if (sa.sa_family != other.sa.sa_family) {
        return false;
    }
    switch (sa.sa_family) {
    case AF_INET:
        return (sa4.sin_port == other.sa4.sin_port)
               && (sa4.sin_addr.s_addr == other.sa4.sin_addr.s_addr);
    case AF_INET6:
        return (sa6.sin6_port == other.sa6.sin6_port)
               && !memcmp(sa6.sin6_addr.s6_addr, other.sa6.sin6_addr.s6_addr, 16);
    default:
        return !memcmp(this, &other, sizeof(*this));
    }

    return false;
}

void sockaddrs::pton(int type, const Anope::string &address, int pport) {
    this->clear();

    switch (type) {
    case AF_INET: {
        int i = inet_pton(type, address.c_str(), &sa4.sin_addr);
        if (i <= 0) {
            this->clear();
        } else {
            sa4.sin_family = type;
            sa4.sin_port = htons(pport);
        }
        break;
    }
    case AF_INET6: {
        int i = inet_pton(type, address.c_str(), &sa6.sin6_addr);
        if (i <= 0) {
            this->clear();
        } else {
            sa6.sin6_family = type;
            sa6.sin6_port = htons(pport);
        }
        break;
    }
    default:
        break;
    }
}

void sockaddrs::ntop(int type, const void *src) {
    char buf[INET6_ADDRSTRLEN];

    if (inet_ntop(type, src, buf, sizeof(buf)) != buf) {
        this->clear();
        return;
    }

    switch (type) {
    case AF_INET:
        sa4.sin_addr = *reinterpret_cast<const in_addr *>(src);
        sa4.sin_family = type;
        return;
    case AF_INET6:
        sa6.sin6_addr = *reinterpret_cast<const in6_addr *>(src);
        sa6.sin6_family = type;
        return;
    default:
        break;
    }

    this->clear();
}

cidr::cidr(const Anope::string &ip) {
    bool ipv6 = ip.find(':') != Anope::string::npos;
    size_t sl = ip.find_last_of('/');

    if (sl == Anope::string::npos) {
        this->cidr_ip = ip;
        this->cidr_len = ipv6 ? 128 : 32;
        this->addr.pton(ipv6 ? AF_INET6 : AF_INET, ip);
    } else {
        Anope::string real_ip = ip.substr(0, sl);
        Anope::string cidr_range = ip.substr(sl + 1);

        this->cidr_ip = real_ip;
        this->cidr_len = ipv6 ? 128 : 32;
        try {
            if (cidr_range.is_pos_number_only()) {
                this->cidr_len = convertTo<unsigned int>(cidr_range);
            }
        } catch (const ConvertException &) { }
        this->addr.pton(ipv6 ? AF_INET6 : AF_INET, real_ip);
    }
}

cidr::cidr(const Anope::string &ip, unsigned char len) {
    bool ipv6 = ip.find(':') != Anope::string::npos;
    this->addr.pton(ipv6 ? AF_INET6 : AF_INET, ip);
    this->cidr_ip = ip;
    this->cidr_len = len;
}

cidr::cidr(const sockaddrs &a, unsigned char len) : addr(a) {
    this->cidr_ip = a.addr();
    this->cidr_len = len;
}

Anope::string cidr::mask() const {
    if ((this->addr.ipv6() && this->cidr_len == 128) || (!this->addr.ipv6()
            && this->cidr_len == 32)) {
        return this->cidr_ip;
    } else {
        return Anope::printf("%s/%d", this->cidr_ip.c_str(), this->cidr_len);
    }
}

bool cidr::match(const sockaddrs &other) {
    if (!valid() || !other.valid()
            || this->addr.sa.sa_family != other.sa.sa_family) {
        return false;
    }

    const uint8_t *ip, *their_ip;
    uint8_t byte, len = this->cidr_len;

    switch (this->addr.sa.sa_family) {
    case AF_INET:
        ip = reinterpret_cast<const uint8_t *>(&this->addr.sa4.sin_addr);
        if (len > 32) {
            len = 32;
        }
        byte = len / 8;
        their_ip = reinterpret_cast<const uint8_t *>(&other.sa4.sin_addr);
        break;
    case AF_INET6:
        ip = reinterpret_cast<const uint8_t *>(&this->addr.sa6.sin6_addr);
        if (len > 128) {
            len = 128;
        }
        byte = len / 8;
        their_ip = reinterpret_cast<const uint8_t *>(&other.sa6.sin6_addr);
        break;
    default:
        return false;
    }

    if (memcmp(ip, their_ip, byte)) {
        return false;
    }

    ip += byte;
    their_ip += byte;

    byte = len % 8;
    if (byte) {
        uint8_t m = ~0 << (8 - byte);
        return (*ip & m) == (*their_ip & m);
    }

    return true;
}

bool cidr::operator<(const cidr &other) const {
    if (this->addr.sa.sa_family != other.addr.sa.sa_family) {
        return this->addr.sa.sa_family < other.addr.sa.sa_family;
    }

    switch (this->addr.sa.sa_family) {
    case AF_INET: {
        unsigned int m = 0xFFFFFFFFU >> (32 - this->cidr_len);

        return (this->addr.sa4.sin_addr.s_addr & m) < (other.addr.sa4.sin_addr.s_addr &
                m);
    }
    case AF_INET6: {
        int i = memcmp(&this->addr.sa6.sin6_addr.s6_addr,
                       &other.addr.sa6.sin6_addr.s6_addr, this->cidr_len / 8);
        if (i || this->cidr_len >= 128) {
            return i < 0;
        }

        // Now all thats left is to compare 'remaining' bits at offset this->cidr_len / 8
        int remaining = this->cidr_len % 8;
        unsigned char m = 0xFF << (8 - remaining);

        return (this->addr.sa6.sin6_addr.s6_addr[this->cidr_len / 8] & m) <
               (other.addr.sa6.sin6_addr.s6_addr[this->cidr_len / 8] & m);
    }
    default:
        throw CoreException("Unknown AFTYPE for cidr");
    }
}

bool cidr::operator==(const cidr &other) const {
    return !(*this < other) && !(other < *this);
}

bool cidr::operator!=(const cidr &other) const {
    return !(*this == other);
}

bool cidr::valid() const {
    return this->addr.valid();
}

size_t cidr::hash::operator()(const cidr &s) const {
    switch (s.addr.sa.sa_family) {
    case AF_INET: {
        unsigned int m = 0xFFFFFFFFU >> (32 - s.cidr_len);
        return s.addr.sa4.sin_addr.s_addr & m;
    }
    case AF_INET6: {
        size_t h = 0;

        for (unsigned i = 0; i < s.cidr_len / 8; ++i) {
            h ^= (s.addr.sa6.sin6_addr.s6_addr[i] << ((i * 8) % sizeof(size_t)));
        }

        int remaining = s.cidr_len % 8;
        unsigned char m = 0xFF << (8 - remaining);

        h ^= s.addr.sa6.sin6_addr.s6_addr[s.cidr_len / 8] & m;

        return h;
    }
    default:
        throw CoreException("Unknown AFTYPE for cidr");
    }
}

int SocketIO::Recv(Socket *s, char *buf, size_t sz) {
    int i = recv(s->GetFD(), buf, sz, 0);
    if (i > 0) {
        TotalRead += i;
    }
    return i;
}

int SocketIO::Send(Socket *s, const char *buf, size_t sz) {
    int i = send(s->GetFD(), buf, sz, 0);
    if (i > 0) {
        TotalWritten += i;
    }
    return i;
}

int SocketIO::Send(Socket *s, const Anope::string &buf) {
    return this->Send(s, buf.c_str(), buf.length());
}

ClientSocket *SocketIO::Accept(ListenSocket *s) {
    sockaddrs conaddr;

    socklen_t size = sizeof(conaddr);
    int newsock = accept(s->GetFD(), &conaddr.sa, &size);

    if (newsock >= 0) {
        ClientSocket *ns = s->OnAccept(newsock, conaddr);
        ns->flags[SF_ACCEPTED] = true;
        ns->OnAccept();
        return ns;
    } else {
        throw SocketException("Unable to accept connection: " + Anope::LastError());
    }
}

SocketFlag SocketIO::FinishAccept(ClientSocket *cs) {
    return SF_ACCEPTED;
}

void SocketIO::Bind(Socket *s, const Anope::string &ip, int port) {
    s->bindaddr.pton(s->IsIPv6() ? AF_INET6 : AF_INET, ip, port);
    if (bind(s->GetFD(), &s->bindaddr.sa, s->bindaddr.size()) == -1) {
        throw SocketException("Unable to bind to address: " + Anope::LastError());
    }
}

void SocketIO::Connect(ConnectionSocket *s, const Anope::string &target,
                       int port) {
    s->flags[SF_CONNECTING] = s->flags[SF_CONNECTED] = false;
    s->conaddr.pton(s->IsIPv6() ? AF_INET6 : AF_INET, target, port);
    int c = connect(s->GetFD(), &s->conaddr.sa, s->conaddr.size());
    if (c == -1) {
        if (!SocketEngine::IgnoreErrno()) {
            s->OnError(Anope::LastError());
        } else {
            SocketEngine::Change(s, true, SF_WRITABLE);
            s->flags[SF_CONNECTING] = true;
        }
    } else {
        s->flags[SF_CONNECTED] = true;
        s->OnConnect();
    }
}

SocketFlag SocketIO::FinishConnect(ConnectionSocket *s) {
    if (s->flags[SF_CONNECTED]) {
        return SF_CONNECTED;
    } else if (!s->flags[SF_CONNECTING]) {
        throw SocketException("SocketIO::FinishConnect called for a socket not connected nor connecting?");
    }

    int optval = 0;
    socklen_t optlen = sizeof(optval);
    if (!getsockopt(s->GetFD(), SOL_SOCKET, SO_ERROR,
                    reinterpret_cast<char *>(&optval), &optlen) && !optval) {
        s->flags[SF_CONNECTED] = true;
        s->flags[SF_CONNECTING] = false;
        s->OnConnect();
        return SF_CONNECTED;
    } else {
        errno = optval;
        s->OnError(optval ? Anope::LastError() : "");
        return SF_DEAD;
    }
}

Socket::Socket() {
    throw CoreException("Socket::Socket() ?");
}

Socket::Socket(int s, bool i, int type) {
    this->io = &NormalSocketIO;
    this->ipv6 = i;
    if (s == -1) {
        this->sock = socket(this->ipv6 ? AF_INET6 : AF_INET, type, 0);
    } else {
        this->sock = s;
    }
    this->SetBlocking(false);
    SocketEngine::Sockets[this->sock] = this;
    SocketEngine::Change(this, true, SF_READABLE);
}

Socket::~Socket() {
    SocketEngine::Change(this, false, SF_READABLE);
    SocketEngine::Change(this, false, SF_WRITABLE);
    anope_close(this->sock);
    this->io->Destroy();
    SocketEngine::Sockets.erase(this->sock);
}

int Socket::GetFD() const {
    return sock;
}

bool Socket::IsIPv6() const {
    return ipv6;
}

bool Socket::SetBlocking(bool state) {
    int f = fcntl(this->GetFD(), F_GETFL, 0);
    if (state) {
        return !fcntl(this->GetFD(), F_SETFL, f & ~O_NONBLOCK);
    } else {
        return !fcntl(this->GetFD(), F_SETFL, f | O_NONBLOCK);
    }
}

void Socket::Bind(const Anope::string &ip, int port) {
    this->io->Bind(this, ip, port);
}

bool Socket::Process() {
    return true;
}

bool Socket::ProcessRead() {
    return true;
}

bool Socket::ProcessWrite() {
    return true;
}

void Socket::ProcessError() {
}

ListenSocket::ListenSocket(const Anope::string &bindip, int port, bool i) {
    this->SetBlocking(false);

    const int op = 1;
    setsockopt(this->GetFD(), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&op), sizeof(op));

    this->bindaddr.pton(i ? AF_INET6 : AF_INET, bindip, port);
    this->io->Bind(this, bindip, port);

    if (listen(sock, SOMAXCONN) == -1) {
        throw SocketException("Unable to listen: " + Anope::LastError());
    }
}

ListenSocket::~ListenSocket() {
}

bool ListenSocket::ProcessRead() {
    try {
        this->io->Accept(this);
    } catch (const SocketException &ex) {
        Log() << ex.GetReason();
    }
    return true;
}

int SocketEngine::GetLastError() {
#ifndef _WIN32
    return errno;
#else
    return WSAGetLastError();
#endif
}

void SocketEngine::SetLastError(int err) {
#ifndef _WIN32
    errno = err;
#else
    WSASetLastError(err);
#endif
}

bool SocketEngine::IgnoreErrno() {
    return GetLastError() == EAGAIN
           || GetLastError() == EWOULDBLOCK
           || GetLastError() == EINTR
           || GetLastError() == EINPROGRESS;
}
