/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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

#include <errno.h>
#include "inspircd_config.h"
#include "socketengine.h"

/* Accept Define */
#ifdef CONFIG_USE_IOCP
/* IOCP wrapper for accept() */
#define _accept(s, addr, addrlen) __accept_socket(s, addr, addrlen, m_acceptEvent)
/* IOCP wrapper for getsockname() */
#define _getsockname(fd, sockptr, socklen) __getsockname(fd, sockptr, socklen, m_acceptEvent)
/* IOCP wrapper for recvfrom() */
#define _recvfrom(s, buf, len, flags, from, fromlen) __recvfrom(s, buf, len, flags, from, fromlen, ((IOCPEngine*)ServerInstance->SE)->udp_ov)
#else
/* No wrapper for recvfrom() */
#define _recvfrom recvfrom
/* No wrapper for accept() */
#define _accept accept
/* No wrapper for getsockname() */
#define _getsockname getsockname
#endif

/* Contains irc-specific definitions */
namespace irc
{
	/** This namespace contains various protocol-independent helper classes.
	 * It also contains some types which are often used by the core and modules
	 * in place of inet_* functions and types.
	 */
	namespace sockets
	{

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
		CoreExport bool MatchCIDRBits(unsigned char* address, unsigned char* mask, unsigned int mask_bits);

		/** Match CIDR, without matching username/nickname parts.
		 *
		 * This function will compare a human-readable address against a human-
		 * readable CIDR mask, for example 1.2.3.4 against 1.2.0.0/16. This
		 * method supports both IPV4 and IPV6 addresses.
		 * @param address The human readable address, e.g. 1.2.3.4
		 * @param cidr_mask The human readable mask, e.g. 1.2.0.0/16
		 * @return True if the mask matches the address
		 */
		CoreExport bool MatchCIDR(const char* address, const char* cidr_mask);

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
		CoreExport bool MatchCIDR(const char* address, const char* cidr_mask, bool match_with_username);

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

		/** Make a socket file descriptor a blocking socket
		 * @param s A valid file descriptor
		 */
		CoreExport void Blocking(int s);

		/** Make a socket file descriptor into a nonblocking socket
		 * @param s A valid file descriptor
		 */
		CoreExport void NonBlocking(int s);

		/** Create a new valid file descriptor using socket()
		 * @return On return this function will return a value >= 0 for success,
		 * or a negative value upon failure (negative values are invalid file
		 * descriptors)
		 */
		CoreExport int OpenTCPSocket(char* addr, int socktype = SOCK_STREAM);
	}
}

/** This class handles incoming connections on client ports.
 * It will create a new userrec for every valid connection
 * and assign it a file descriptor.
 */
class CoreExport ListenSocket : public EventHandler
{
 protected:
	/** The creator/owner of this object
	 */
	InspIRCd* ServerInstance;
	/** Socket description (shown in stats p) */
	std::string desc;
	/** Socket address family */
	int family;
	/** Address socket is bound to */
	std::string bind_addr;
	/** Port socket is bound to */
	int bind_port;
 public:
	/** Create a new listening socket
	 */
	ListenSocket(InspIRCd* Instance, int port, char* addr);
	/** Handle an I/O event
	 */
	void HandleEvent(EventType et, int errornum = 0);
	/** Close the socket
	 */
	~ListenSocket();
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
	int GetPort()
	{
		return bind_port;
	}
	/** Get IP address socket is bound to
	 */
	std::string &GetIP()
	{
		return bind_addr;
	}
};

#endif

