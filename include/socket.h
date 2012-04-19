/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_SOCKET_H
#define INSPIRCD_SOCKET_H

#ifndef WIN32

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
#include "socketengine.h"

/* Contains irc-specific definitions */
namespace irc
{
	/** This namespace contains various protocol-independent helper classes.
	 * It also contains some types which are often used by the core and modules
	 * in place of inet_* functions and types.
	 */
	namespace sockets
	{

		typedef union {
			struct sockaddr sa;
			struct sockaddr_in in4;
			struct sockaddr_in6 in6;
		} sockaddrs;

	/* macros to the relevant system address description structs */
#ifdef IPV6
		/** insp_sockaddr for ipv6
		 */
		typedef struct sockaddr_in6 insp_sockaddr;
		/** insp_inaddr for ipv6
		 */
		typedef struct in6_addr     insp_inaddr;
#define AF_FAMILY AF_INET6
#define PF_PROTOCOL PF_INET6

#else
		/** insp_sockaddr for ipv4
		 */
		typedef struct sockaddr_in  insp_sockaddr;
		/** insp_inaddr for ipv4
		 */
		typedef struct in_addr      insp_inaddr;
#define AF_FAMILY AF_INET
#define PF_PROTOCOL PF_INET

#endif
		/** Match raw binary data using CIDR rules.
		 *
		 * This function will use binary comparison to compare the
		 * two bit sequences, address and mask, up to mask_bits
		 * bits in size. If they match, it will return true.
		 * @param address The whole address, of 4 or 16 bytes in length
		 * @param mask The mask, from 1 to 16 bytes in length, anything
		 * from 1 to 128 bits of which is significant
		 * @param mask_Bits How many bits of the mask parameter are significant
		 * for this comparison.
		 * @returns True if the first mask_bits of address matches the first
		 * mask_bits of mask.
		 */
		CoreExport bool MatchCIDRBits(const unsigned char* address, const unsigned char* mask, unsigned int mask_bits);

		/** Match CIDR, without matching username/nickname parts.
		 *
		 * This function will compare a human-readable address against a human-
		 * readable CIDR mask, for example 1.2.3.4 against 1.2.0.0/16. This
		 * method supports both IPV4 and IPV6 addresses.
		 * @param address The human readable address, e.g. 1.2.3.4
		 * @param cidr_mask The human readable mask, e.g. 1.2.0.0/16
		 * @return True if the mask matches the address
		 */
		CoreExport bool MatchCIDR(const std::string &address, const std::string &cidr_mask);

		/** Match CIDR, including an optional username/nickname part.
		 *
		 * This function will compare a human-readable address (plus
		 * optional username and nickname) against a human-readable
		 * CIDR mask, for example joe!bloggs\@1.2.3.4 against
		 * *!bloggs\@1.2.0.0/16. This method supports both IPV4 and
		 * IPV6 addresses.
		 * @param address The human readable address, e.g. fred\@1.2.3.4
		 * @param cidr_mask The human readable mask, e.g. *\@1.2.0.0/16
		 * @return True if the mask matches the address
		 */
		CoreExport bool MatchCIDR(const std::string &address, const std::string &cidr_mask, bool match_with_username);

		/** Convert an insp_inaddr into human readable form.
		 *
		 * @param n An insp_inaddr (IP address) structure
		 * @return A human-readable address. IPV6 addresses
		 * will be shortened to remove fields which are 0.
		 */
		CoreExport const char* insp_ntoa(insp_inaddr n);

		/** Convert a human-readable address into an insp_inaddr.
		 *
		 * @param a A human-readable address
		 * @param n An insp_inaddr struct which the result
		 * will be copied into on success.
		 * @return This method will return a negative value if address
		 * does not contain a valid address family. 0 if the address is
		 * does not contain a valid string representing a valid network
		 * address. A positive value is returned if the network address
		 * was successfully converted.

		 * or any other number upon failure.
		 */
		CoreExport int insp_aton(const char* a, insp_inaddr* n);

		/** Create a new valid file descriptor using socket()
		 * @return On return this function will return a value >= 0 for success,
		 * or a negative value upon failure (negative values are invalid file
		 * descriptors)
		 */
		CoreExport int OpenTCPSocket(const char* addr, int socktype = SOCK_STREAM);

		/** Return the size of the structure for syscall passing */
		CoreExport int sa_size(irc::sockets::sockaddrs& sa);

		/** Convert an address-port pair into a binary sockaddr
		 * @param addr The IP address, IPv4 or IPv6
		 * @param port The port, 0 for unspecified
		 * @param sa The structure to place the result in. Will be zeroed prior to conversion
		 * @return true if the conversion was successful, false if not.
		 */
		CoreExport int aptosa(const char* addr, int port, irc::sockets::sockaddrs* sa);
		/** Convert a binary sockaddr to an address-port pair
		 * @param sa The structure to convert
		 * @param addr the IP address
		 * @param port the port
		 * @return true if the conversion was successful, false if unknown address family
		 */
		CoreExport int satoap(const irc::sockets::sockaddrs* sa, std::string& addr, int &port);
	}
}



/** This class handles incoming connections on client ports.
 * It will create a new User for every valid connection
 * and assign it a file descriptor.
 */
class CoreExport ListenSocketBase : public EventHandler
{
 protected:
	/** The creator/owner of this object
	 */
	InspIRCd* ServerInstance;
	/** Socket description (shown in stats p) */
	std::string desc;

	/** Address socket is bound to */
	std::string bind_addr;
	/** Port socket is bound to */
	int bind_port;

	/** The client address if the most recently connected client.
	 * Should only be used when accepting a new client.
	 */
	static irc::sockets::sockaddrs client;
	/** The server address used by the most recently connected client.
	 * This may differ from the bind address by having a nonzero address,
	 * if the port is wildcard bound, or being IPv4 on a 6to4 IPv6 port.
	 * The address family will always match that of "client"
	 */
	static irc::sockets::sockaddrs server;
 public:
	/** Create a new listening socket
	 */
	ListenSocketBase(InspIRCd* Instance, int port, const std::string &addr);
	/** Handle an I/O event
	 */
	void HandleEvent(EventType et, int errornum = 0);
	/** Close the socket
	 */
	~ListenSocketBase();
	/** Set descriptive text
	 */
	void SetDescription(const std::string &description)
	{
		desc = description;
	}
	/** Get description for socket
	 */
	const std::string& GetDescription()
	{
		return desc;
	}
	/** Get port number for socket
	 */
	int GetPort() { return bind_port; }

	/** Get IP address socket is bound to
	 */
	const std::string &GetIP() { return bind_addr; }

	/** Handles sockets internals crap of a connection, convenience wrapper really
	 */
	void AcceptInternal();

	/** Called when a new connection has successfully been accepted on this listener.
	 * @param ipconnectedto The IP address the connection arrived on
	 * @param fd The file descriptor of the new connection
	 * @param incomingip The IP from which the connection was made
	 */
	virtual void OnAcceptReady(const std::string &ipconnectedto, int fd, const std::string &incomingip) = 0;
};

class CoreExport ClientListenSocket : public ListenSocketBase
{
	virtual void OnAcceptReady(const std::string &ipconnectedto, int fd, const std::string &incomingip);
 public:
	ClientListenSocket(InspIRCd* Instance, int port, const std::string &addr) : ListenSocketBase(Instance, port, addr) { }
};

#endif

