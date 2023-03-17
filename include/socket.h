/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2012 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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

#ifndef _WIN32

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#else

#include "inspircd_win32wrapper.h"

#endif

#include <cerrno>

/* Contains irc-specific definitions */
namespace irc {
/** This namespace contains various protocol-independent helper classes.
 * It also contains some types which are often used by the core and modules
 * in place of inet_* functions and types.
 */
namespace sockets {
union CoreExport sockaddrs {
    struct sockaddr sa;
    struct sockaddr_in in4;
    struct sockaddr_in6 in6;
    struct sockaddr_un un;
    /** Return the family of the socket (e.g. AF_INET). */
    int family() const;
    /** Return the size of the structure for syscall passing */
    socklen_t sa_size() const;
    /** Return port number or -1 if invalid */
    int port() const;
    /** Return IP only */
    std::string addr() const;
    /** Return human-readable IP/port pair */
    std::string str() const;
    bool operator==(const sockaddrs& other) const;
    inline bool operator!=(const sockaddrs& other) const {
        return !(*this == other);
    }
};

struct CoreExport cidr_mask {
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
    cidr_mask(const irc::sockets::sockaddrs& addr, unsigned char len);
    /** Equality of bits, type, and length */
    bool operator==(const cidr_mask& other) const;
    /** Ordering defined for maps */
    bool operator<(const cidr_mask& other) const;
    /** Match within this CIDR? */
    bool match(const irc::sockets::sockaddrs& addr) const;
    /** Human-readable string */
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
 * @param match_with_username Does the  mask include a nickname segment?
 * @return True if the mask matches the address
 */
CoreExport bool MatchCIDR(const std::string &address,
                          const std::string &cidr_mask, bool match_with_username);

/** Convert an address-port pair into a binary sockaddr
 * @param addr The IP address, IPv4 or IPv6
 * @param port The port, 0 for unspecified
 * @param sa The structure to place the result in. Will be zeroed prior to conversion
 * @return true if the conversion was successful, false if not.
 */
CoreExport bool aptosa(const std::string& addr, int port,
                       irc::sockets::sockaddrs& sa);

/** Convert a UNIX socket path to a binary sockaddr.
 * @param path The path to the UNIX socket.
 * @param sa The structure to place the result in. Will be zeroed prior to conversion.
 * @return True if the conversion was successful; otherwise, false.
 */
CoreExport bool untosa(const std::string& path, irc::sockets::sockaddrs& sa);

/** Determines whether the specified file is a UNIX socket.
 * @param file The path to the file to check.
 * @return True if the file is a UNIX socket; otherwise, false.
 */
CoreExport bool isunix(const std::string& file);
}
}

/** Represents information about a failed port binding. */
struct CoreExport FailedPort {
    /** The error which happened during binding. */
    int error;

    /** The endpoint on which we were attempting to bind. */
    irc::sockets::sockaddrs sa;

    /** The config tag that the listener was created from. */
    ConfigTag* tag;

    FailedPort(int err, irc::sockets::sockaddrs& ep, ConfigTag* cfg)
        : error(err)
        , sa(ep)
        , tag(cfg) {
    }
};

/** A list of failed port bindings, used for informational purposes on startup */
typedef std::vector<FailedPort> FailedPortList;

#include "socketengine.h"

/** This class handles incoming connections on client ports.
 * It will create a new User for every valid connection
 * and assign it a file descriptor.
 */
class CoreExport ListenSocket : public EventHandler {
  public:
    reference<ConfigTag> bind_tag;
    const irc::sockets::sockaddrs bind_sa;

    class IOHookProvRef : public dynamic_reference_nocheck<IOHookProvider> {
      public:
        IOHookProvRef()
            : dynamic_reference_nocheck<IOHookProvider>(NULL, std::string()) {
        }
    };

    typedef TR1NS::array<IOHookProvRef, 2> IOHookProvList;

    /** IOHook providers for handling connections on this socket,
     * may be empty.
     */
    IOHookProvList iohookprovs;

    /** Create a new listening socket
     */
    ListenSocket(ConfigTag* tag, const irc::sockets::sockaddrs& bind_to);
    /** Close the socket
     */
    ~ListenSocket();

    /** Handles new connections, called by the socket engine
     */
    void OnEventHandlerRead() CXX11_OVERRIDE;

    /** Inspects the bind block belonging to this socket to set the name of the IO hook
     * provider which this socket will use for incoming connections.
     */
    void ResetIOHookProvider();
};
