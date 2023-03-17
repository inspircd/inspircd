/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2011 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 John Brooks <special@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#include "inspircd.h"

bool InspIRCd::BindPort(ConfigTag* tag, const irc::sockets::sockaddrs& sa,
                        std::vector<ListenSocket*>& old_ports) {
    for (std::vector<ListenSocket*>::iterator n = old_ports.begin();
            n != old_ports.end(); ++n) {
        if ((**n).bind_sa == sa) {
            // Replace tag, we know addr and port match, but other info (type, ssl) may not.
            ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT,
                                      "Replacing listener on %s from old tag at %s with new tag from %s",
                                      sa.str().c_str(), (*n)->bind_tag->getTagLocation().c_str(),
                                      tag->getTagLocation().c_str());
            (*n)->bind_tag = tag;
            (*n)->ResetIOHookProvider();

            old_ports.erase(n);
            return true;
        }
    }

    ListenSocket* ll = new ListenSocket(tag, sa);
    if (!ll->HasFd()) {
        ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT,
                                  "Failed to listen on %s from tag at %s: %s",
                                  sa.str().c_str(), tag->getTagLocation().c_str(), strerror(errno));
        delete ll;
        return false;
    }

    ServerInstance->Logs->Log("SOCKET", LOG_DEFAULT,
                              "Added a listener on %s from tag at %s", sa.str().c_str(),
                              tag->getTagLocation().c_str());
    ports.push_back(ll);
    return true;
}

size_t InspIRCd::BindPorts(FailedPortList& failed_ports) {
    size_t bound = 0;
    std::vector<ListenSocket*> old_ports(ports.begin(), ports.end());

    ConfigTagList tags = ServerInstance->Config->ConfTags("bind");
    for (ConfigIter i = tags.first; i != tags.second; ++i) {
        ConfigTag* tag = i->second;

        // Are we creating a TCP/IP listener?
        const std::string address = tag->getString("address");
        const std::string portlist = tag->getString("port");
        if (!address.empty() || !portlist.empty()) {
            // InspIRCd supports IPv4 and IPv6 natively; no 4in6 required.
            if (strncasecmp(address.c_str(), "::ffff:", 7) == 0) {
                this->Logs->Log("SOCKET", LOG_DEFAULT,
                                "Using 4in6 (::ffff:) isn't recommended. You should bind IPv4 addresses directly instead.");
            }

            // A TCP listener with no ports is not very useful.
            if (portlist.empty())
                this->Logs->Log("SOCKET", LOG_DEFAULT,
                                "TCP listener on %s at %s has no ports specified!",
                                address.empty() ? "*" : address.c_str(), tag->getTagLocation().c_str());

            irc::portparser portrange(portlist, false);
            for (int port; (port = portrange.GetToken()); ) {
                irc::sockets::sockaddrs bindspec;
                if (!irc::sockets::aptosa(address, port, bindspec)) {
                    continue;
                }

                if (!BindPort(tag, bindspec, old_ports)) {
                    failed_ports.push_back(FailedPort(errno, bindspec, tag));
                } else {
                    bound++;
                }
            }
            continue;
        }

#ifndef _WIN32
        // Are we creating a UNIX listener?
        const std::string path = tag->getString("path");
        if (!path.empty()) {
            // Expand the path relative to the config directory.
            const std::string fullpath = ServerInstance->Config->Paths.PrependRuntime(path);

            // UNIX socket paths are length limited to less than PATH_MAX.
            irc::sockets::sockaddrs bindspec;
            if (fullpath.length() > std::min(ServerInstance->Config->Limits.MaxHost,
                                             sizeof(bindspec.un.sun_path) - 1)) {
                this->Logs->Log("SOCKET", LOG_DEFAULT,
                                "UNIX listener on %s at %s specified a path that is too long!",
                                fullpath.c_str(), tag->getTagLocation().c_str());
                continue;
            }

            // Check for characters which are problematic in the IRC message format.
            if (fullpath.find_first_of("\n\r\t!@: ") != std::string::npos) {
                this->Logs->Log("SOCKET", LOG_DEFAULT,
                                "UNIX listener on %s at %s specified a path containing invalid characters!",
                                fullpath.c_str(), tag->getTagLocation().c_str());
                continue;
            }

            irc::sockets::untosa(fullpath, bindspec);
            if (!BindPort(tag, bindspec, old_ports)) {
                failed_ports.push_back(FailedPort(errno, bindspec, tag));
            } else {
                bound++;
            }
        }
#endif
    }

    std::vector<ListenSocket*>::iterator n = ports.begin();
    for (std::vector<ListenSocket*>::iterator o = old_ports.begin();
            o != old_ports.end(); ++o) {
        while (n != ports.end() && *n != *o) {
            n++;
        }
        if (n == ports.end()) {
            this->Logs->Log("SOCKET", LOG_DEFAULT,
                            "Port bindings slipped out of vector, aborting close!");
            break;
        }

        this->Logs->Log("SOCKET", LOG_DEFAULT,
                        "Port binding %s was removed from the config file, closing.",
                        (**n).bind_sa.str().c_str());
        delete *n;

        // this keeps the iterator valid, pointing to the next element
        n = ports.erase(n);
    }

    return bound;
}

bool irc::sockets::aptosa(const std::string& addr, int port,
                          irc::sockets::sockaddrs& sa) {
    memset(&sa, 0, sizeof(sa));
    if (addr.empty() || addr.c_str()[0] == '*') {
        if (ServerInstance->Config->WildcardIPv6) {
            sa.in6.sin6_family = AF_INET6;
            sa.in6.sin6_port = htons(port);
        } else {
            sa.in4.sin_family = AF_INET;
            sa.in4.sin_port = htons(port);
        }
        return true;
    } else if (inet_pton(AF_INET, addr.c_str(), &sa.in4.sin_addr) > 0) {
        sa.in4.sin_family = AF_INET;
        sa.in4.sin_port = htons(port);
        return true;
    } else if (inet_pton(AF_INET6, addr.c_str(), &sa.in6.sin6_addr) > 0) {
        sa.in6.sin6_family = AF_INET6;
        sa.in6.sin6_port = htons(port);
        return true;
    }
    return false;
}

bool irc::sockets::untosa(const std::string& path,
                          irc::sockets::sockaddrs& sa) {
    memset(&sa, 0, sizeof(sa));
    if (path.length() >= sizeof(sa.un.sun_path)) {
        return false;
    }

    sa.un.sun_family = AF_UNIX;
    memcpy(&sa.un.sun_path, path.c_str(), path.length() + 1);
    return true;
}

bool irc::sockets::isunix(const std::string& file) {
#ifndef _WIN32
    struct stat sb;
    if (stat(file.c_str(), &sb) == 0 && S_ISSOCK(sb.st_mode)) {
        return true;
    }
#endif
    return false;
}


int irc::sockets::sockaddrs::family() const {
    return sa.sa_family;
}

int irc::sockets::sockaddrs::port() const {
    switch (family()) {
    case AF_INET:
        return ntohs(in4.sin_port);

    case AF_INET6:
        return ntohs(in6.sin6_port);

    case AF_UNIX:
        return 0;
    }

    // If we have reached this point then we have encountered a bug.
    ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                              "BUG: irc::sockets::sockaddrs::port(): socket type %d is unknown!", family());
    return 0;
}

std::string irc::sockets::sockaddrs::addr() const {
    switch (family()) {
    case AF_INET:
        char ip4addr[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, (void*)&in4.sin_addr, ip4addr, sizeof(ip4addr))) {
            return "0.0.0.0";
        }
        return ip4addr;

    case AF_INET6:
        char ip6addr[INET6_ADDRSTRLEN];
        if (!inet_ntop(AF_INET6, (void*)&in6.sin6_addr, ip6addr, sizeof(ip6addr))) {
            return "0:0:0:0:0:0:0:0";
        }
        return ip6addr;

    case AF_UNIX:
        return un.sun_path;
    }

    // If we have reached this point then we have encountered a bug.
    ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                              "BUG: irc::sockets::sockaddrs::addr(): socket type %d is unknown!", family());
    return "<unknown>";
}

std::string irc::sockets::sockaddrs::str() const {
    switch (family()) {
    case AF_INET:
        char ip4addr[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, (void*)&in4.sin_addr, ip4addr, sizeof(ip4addr))) {
            strcpy(ip4addr, "0.0.0.0");
        }
        return InspIRCd::Format("%s:%u", ip4addr, ntohs(in4.sin_port));

    case AF_INET6:
        char ip6addr[INET6_ADDRSTRLEN];
        if (!inet_ntop(AF_INET6, (void*)&in6.sin6_addr, ip6addr, sizeof(ip6addr))) {
            strcpy(ip6addr, "0:0:0:0:0:0:0:0");
        }
        return InspIRCd::Format("[%s]:%u", ip6addr, ntohs(in6.sin6_port));

    case AF_UNIX:
        return un.sun_path;
    }

    // If we have reached this point then we have encountered a bug.
    ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                              "BUG: irc::sockets::sockaddrs::str(): socket type %d is unknown!", family());
    return "<unknown>";
}

socklen_t irc::sockets::sockaddrs::sa_size() const {
    switch (family()) {
    case AF_INET:
        return sizeof(in4);

    case AF_INET6:
        return sizeof(in6);

    case AF_UNIX:
        return sizeof(un);
    }

    // If we have reached this point then we have encountered a bug.
    ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                              "BUG: irc::sockets::sockaddrs::sa_size(): socket type %d is unknown!",
                              family());
    return 0;
}

bool irc::sockets::sockaddrs::operator==(const irc::sockets::sockaddrs& other)
const {
    if (family() != other.family()) {
        return false;
    }

    switch (family()) {
    case AF_INET:
        return (in4.sin_port == other.in4.sin_port)
               && (in4.sin_addr.s_addr == other.in4.sin_addr.s_addr);

    case AF_INET6:
        return (in6.sin6_port == other.in6.sin6_port)
               && !memcmp(in6.sin6_addr.s6_addr, other.in6.sin6_addr.s6_addr, 16);

    case AF_UNIX:
        return !strcmp(un.sun_path, other.un.sun_path);
    }

    // If we have reached this point then we have encountered a bug.
    ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                              "BUG: irc::sockets::sockaddrs::operator==(): socket type %d is unknown!",
                              family());
    return !memcmp(this, &other, sizeof(*this));
}

static void sa2cidr(irc::sockets::cidr_mask& cidr,
                    const irc::sockets::sockaddrs& sa, unsigned char range) {
    const unsigned char* base;
    unsigned char target_byte;

    memset(cidr.bits, 0, sizeof(cidr.bits));

    cidr.type = sa.family();
    switch (cidr.type) {
    case AF_UNIX:
        // XXX: UNIX sockets don't support CIDR. This fix is non-ideal but I can't
        // really think of another way to handle it.
        cidr.length = 0;
        return;

    case AF_INET:
        cidr.length = range > 32 ? 32 : range;
        target_byte = sizeof(sa.in4.sin_addr);
        base = (unsigned char*)&sa.in4.sin_addr;
        break;

    case AF_INET6:
        cidr.length = range > 128 ? 128 : range;
        target_byte = sizeof(sa.in6.sin6_addr);
        base = (unsigned char*)&sa.in6.sin6_addr;
        break;

    default:
        // If we have reached this point then we have encountered a bug.
        ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                                  "BUG: sa2cidr(): socket type %d is unknown!", cidr.type);
        cidr.length = 0;
        return;
    }

    unsigned int border = cidr.length / 8;
    unsigned int bitmask = (0xFF00 >> (range & 7)) & 0xFF;
    for(unsigned int i=0; i < target_byte; i++) {
        if (i < border) {
            cidr.bits[i] = base[i];
        } else if (i == border) {
            cidr.bits[i] = base[i] & bitmask;
        } else {
            return;
        }
    }
}

irc::sockets::cidr_mask::cidr_mask(const irc::sockets::sockaddrs& sa,
                                   unsigned char range) {
    sa2cidr(*this, sa, range);
}

irc::sockets::cidr_mask::cidr_mask(const std::string& mask) {
    std::string::size_type bits_chars = mask.rfind('/');
    irc::sockets::sockaddrs sa;

    if (bits_chars == std::string::npos) {
        irc::sockets::aptosa(mask, 0, sa);
        sa2cidr(*this, sa, 128);
    } else {
        unsigned char range = ConvToNum<unsigned char>(mask.substr(bits_chars + 1));
        irc::sockets::aptosa(mask.substr(0, bits_chars), 0, sa);
        sa2cidr(*this, sa, range);
    }
}

std::string irc::sockets::cidr_mask::str() const {
    irc::sockets::sockaddrs sa;
    sa.sa.sa_family = type;

    unsigned char* base;
    size_t len;
    unsigned char maxlen;
    switch (type) {
    case AF_INET:
        base = (unsigned char*)&sa.in4.sin_addr;
        len = 4;
        maxlen = 32;
        break;

    case AF_INET6:
        base = (unsigned char*)&sa.in6.sin6_addr;
        len = 16;
        maxlen = 128;
        break;

    case AF_UNIX:
        // TODO: make bits a vector<uint8_t> so we can return the actual path here.
        return "/*";

    default:
        // If we have reached this point then we have encountered a bug.
        ServerInstance->Logs->Log("SOCKET", LOG_DEBUG,
                                  "BUG: irc::sockets::cidr_mask::str(): socket type %d is unknown!", type);
        return "<unknown>";
    }

    memcpy(base, bits, len);

    std::string value = sa.addr();
    if (length < maxlen) {
        value.push_back('/');
        value.append(ConvToStr(static_cast<uint16_t>(length)));
    }
    return value;
}

bool irc::sockets::cidr_mask::operator==(const cidr_mask& other) const {
    return type == other.type && length == other.length &&
           0 == memcmp(bits, other.bits, 16);
}

bool irc::sockets::cidr_mask::operator<(const cidr_mask& other) const {
    if (type != other.type) {
        return type < other.type;
    }
    if (length != other.length) {
        return length < other.length;
    }
    return memcmp(bits, other.bits, 16) < 0;
}

bool irc::sockets::cidr_mask::match(const irc::sockets::sockaddrs& addr) const {
    if (addr.family() != type) {
        return false;
    }
    irc::sockets::cidr_mask tmp(addr, length);
    return tmp == *this;
}
