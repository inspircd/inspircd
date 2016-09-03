/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
 *   Copyright (C) 2006 William Pitcock <nenolod@dereferenced.org>
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
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#else

#include "inspircd_win32wrapper.h"

#endif

#include <cerrno>

/* Contains irc-specific definitions */
namespace irc
{
	/** This namespace contains various protocol-independent helper classes.
	 * It also contains some types which are often used by the core and modules
	 * in place of inet_* functions and types.
	 */
	namespace sockets
	{
		union CoreExport sockaddrs
		{
			struct sockaddr sa;
			struct sockaddr_in in4;
			struct sockaddr_in6 in6;
			/** Return the size of the structure for syscall passing */
			int sa_size() const;
			/** Return port number or -1 if invalid */
			int port() const;
			/** Return IP only */
			std::string addr() const;
			/** Return human-readable IP/port pair */
			std::string str() const;
			bool operator==(const sockaddrs& other) const;
			inline bool operator!=(const sockaddrs& other) const { return !(*this == other); }
		};

		struct CoreExport cidr_mask
		{
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
			cidr_mask(const irc::sockets::sockaddrs& addr, int len);
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
		CoreExport bool MatchCIDR(const std::string &address, const std::string &cidr_mask, bool match_with_username);

		/** Convert an address-port pair into a binary sockaddr
		 * @param addr The IP address, IPv4 or IPv6
		 * @param port The port, 0 for unspecified
		 * @param sa The structure to place the result in. Will be zeroed prior to conversion
		 * @return true if the conversion was successful, false if not.
		 */
		CoreExport bool aptosa(const std::string& addr, int port, irc::sockets::sockaddrs& sa);

		/** Convert a binary sockaddr to an address-port pair
		 * @param sa The structure to convert
		 * @param addr the IP address
		 * @param port the port
		 * @return true if the conversion was successful, false if unknown address family
		 */
		CoreExport bool satoap(const irc::sockets::sockaddrs& sa, std::string& addr, int &port);
	}
}

#include "socketengine.h"
/** This class handles incoming connections on client ports.
 * It will create a new User for every valid connection
 * and assign it a file descriptor.
 */
class CoreExport ListenSocket : public EventHandler
{
 public:
	reference<ConfigTag> bind_tag;
	std::string bind_addr;
	int bind_port;
	/** Human-readable bind description */
	std::string bind_desc;

	class IOHookProvRef : public dynamic_reference_nocheck<IOHookProvider>
	{
	 public:
		IOHookProvRef()
			: dynamic_reference_nocheck<IOHookProvider>(NULL, std::string())
		{
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
