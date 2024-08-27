/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012-2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
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
# include <netinet/in.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <sys/un.h>
#else
# include <afunix.h>
typedef USHORT in_port_t;
typedef ADDRESS_FAMILY sa_family_t;
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
		/** Represents a socket address. This can be an IP/port pair or a UNIX socket path. */
		union CoreExport sockaddrs
		{
			struct sockaddr sa;
			struct sockaddr_in in4;
			struct sockaddr_in6 in6;
			struct sockaddr_un un;

			/** Initializes this sockaddrs optionally as an unspecified socket address. */
			explicit sockaddrs(bool initialize = true);

			/** Returns the address segment of the socket address as a string. */
			std::string addr() const;

			/** Returns the family of the socket address (e.g. AF_INET). */
			sa_family_t family() const;

			/** Store an IP address or UNIX socket path in this socket address.
			 * @param addr An IPv4 address, IPv6 address, or UNIX socket path.
			 * @return True if the IP address or UNIX socket paht was stored in this socket address; otherwise, false.
			 */
			inline bool from(const std::string& addr) { return addr.find('/') == std::string::npos ? from_ip(addr) : from_unix(addr); }

			/** Store an IP address in this socket address.
			 * @param addr An IPv4 or IPv6 address.
			 * @return True if the IP was stored in this socket address; otherwise, false.
			 */
			inline bool from_ip(const std::string& addr) { return from_ip_port(addr, 0); }

			/** Store an IP address and TCP port pair in this socket address.
			 * @param addr An IPv4 or IPv6 address.
			 * @param port A TCP port.
			 * @return True if the IP/port was stored in this socket address; otherwise, false.
			 */
			bool from_ip_port(const std::string& addr, in_port_t port);

			/** Store a UNIX socket path in this socket address.
			 * @param path A path to a UNIX socket.
			 * @return True if the UNIX socket path was stored in this socket address; otherwise, false.
			 */
			bool from_unix(const std::string& path);

			/** Determines whether this socket address is a local endpoint. */
			bool is_local() const;

			/** Determines whether this socket address is an IPv4 or IPv6 address. */
			inline bool is_ip() const { return family() == AF_INET || family() == AF_INET6; }

			/** Returns the TCP port number of the socket address or 0 if not relevant to this family. */
			in_port_t port() const;

			/** Returns the size of the structure for use in networking syscalls. */
			socklen_t sa_size() const;

			/** Returns the whole socket address as a string. */
			std::string str() const;

			/** Determines if this socket address refers to the same endpoint as another socket address. */
			bool operator==(const sockaddrs& other) const;
			inline bool operator!=(const sockaddrs& other) const { return !(*this == other); }
		};

		struct CoreExport cidr_mask
		{
			/** Type, AF_INET or AF_INET6 */
			sa_family_t type;
			/** Length of the mask in bits (0-128) */
			unsigned char length;
			/** Raw bits. Unused bits must be zero */
			unsigned char bits[16];

			cidr_mask() = default;
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
		CoreExport bool MatchCIDR(const std::string& address, const std::string& cidr_mask, bool match_with_username);

		/** Determines whether the specified file is a UNIX socket.
		 * @param file The path to the file to check.
		 * @return True if the file is a UNIX socket; otherwise, false.
		 */
		CoreExport bool isunix(const std::string& file);
	}
}

/** Represents information about a failed port binding. */
struct CoreExport FailedPort final
{
	/** The error which happened during binding. */
	const std::string error;

	/** The endpoint on which we were attempting to bind. */
	const irc::sockets::sockaddrs sa;

	/** The config tag that the listener was created from. */
	const std::shared_ptr<ConfigTag> tag;

	FailedPort(const std::string& err, irc::sockets::sockaddrs& addr, const std::shared_ptr<ConfigTag>& cfg)
		: error(err)
		, sa(addr)
		, tag(cfg)
	{
	}

	FailedPort(const std::string& err, const std::shared_ptr<ConfigTag>& cfg)
		: error(err)
		, sa(true)
		, tag(cfg)
	{
	}
};

/** A list of failed port bindings, used for informational purposes on startup */
typedef std::vector<FailedPort> FailedPortList;

#include "socketengine.h"

/** This class handles incoming connections on client ports.
 * It will create a new User for every valid connection
 * and assign it a file descriptor.
 */
class CoreExport ListenSocket final
	: public EventHandler
{
public:
	std::shared_ptr<ConfigTag> bind_tag;
	const irc::sockets::sockaddrs bind_sa;
	const int bind_protocol;

	class IOHookProvRef : public dynamic_reference_nocheck<IOHookProvider>
	{
	public:
		IOHookProvRef()
			: dynamic_reference_nocheck<IOHookProvider>(nullptr, std::string())
		{
		}
	};

	typedef std::array<IOHookProvRef, 2> IOHookProvList;

	/** IOHook providers for handling connections on this socket,
	 * may be empty.
	 */
	IOHookProvList iohookprovs;

	/** Create a new listening socket
	 */
	ListenSocket(const std::shared_ptr<ConfigTag>& tag, const irc::sockets::sockaddrs& bind_to, sa_family_t protocol);
	/** Close the socket
	 */
	~ListenSocket() override;

	/** Handles new connections, called by the socket engine
	 */
	void OnEventHandlerRead() override;

	/** Inspects the bind block belonging to this socket to set the name of the IO hook
	 * provider which this socket will use for incoming connections.
	 */
	void ResetIOHookProvider();
};
