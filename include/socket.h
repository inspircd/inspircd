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

/* macros to the relevant system address description structs */
#ifdef IPV6

typedef struct sockaddr_in6 insp_sockaddr;
typedef struct in6_addr     insp_inaddr;
#define AF_FAMILY AF_INET6
#define PF_PROTOCOL PF_INET6

#else

typedef struct sockaddr_in  insp_sockaddr;
typedef struct in_addr      insp_inaddr;
#define AF_FAMILY AF_INET
#define PF_PROTOCOL PF_INET

#endif

bool MatchCIDRBits(unsigned char* address, unsigned char* mask, unsigned int mask_bits);
bool MatchCIDR(const char* address, const char* cidr_mask);

const char* insp_ntoa(insp_inaddr n);
int insp_aton(const char* a, insp_inaddr* n);

int OpenTCPSocket(); 
bool BindSocket(int sockfd, insp_sockaddr client, insp_sockaddr server, int port, char* addr);
int BindPorts(bool bail);

#endif
