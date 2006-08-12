/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_SOCKET_H
#define INSPIRCD_SOCKET_H

#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <errno.h>
#include "inspircd_config.h"

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
		bool MatchCIDRBits(unsigned char* address, unsigned char* mask, unsigned int mask_bits);

		/** Match CIDR, without matching username/nickname parts.
		 *
		 * This function will compare a human-readable address against a human-
		 * readable CIDR mask, for example 1.2.3.4 against 1.2.0.0/16. This
		 * method supports both IPV4 and IPV6 addresses.
		 * @param address The human readable address, e.g. 1.2.3.4
		 * @param cidr_mask The human readable mask, e.g. 1.2.0.0/16
		 * @return True if the mask matches the address
		 */
		bool MatchCIDR(const char* address, const char* cidr_mask);

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
		bool MatchCIDR(const char* address, const char* cidr_mask, bool match_with_username);

		/** Convert an insp_inaddr into human readable form.
		 * 
		 * @param n An insp_inaddr (IP address) structure
		 * @return A human-readable address. IPV6 addresses
		 * will be shortened to remove fields which are 0.
		 */
		const char* insp_ntoa(insp_inaddr n);

		/** Convert a human-readable address into an insp_inaddr.
		 * 
		 * @param a A human-readable address
		 * @param n An insp_inaddr struct which the result
		 * will be copied into on success.
		 * @return This function will return 0 upon success,
		 * or any other number upon failure.
		 */
		int insp_aton(const char* a, insp_inaddr* n);

		/** Make a socket file descriptor a blocking socket
		 * @param s A valid file descriptor
		 */
		void Blocking(int s);

		/** Make a socket file descriptor into a nonblocking socket
		 * @param s A valid file descriptor
		 */
		void NonBlocking(int s);

		/** Create a new valid file descriptor using socket()
		 * @return On return this function will return a value >= 0 for success,
		 * or a negative value upon failure (negative values are invalid file
		 * descriptors)
		 */
		int OpenTCPSocket(); 
	};
};

#endif
