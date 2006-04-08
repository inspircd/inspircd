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

/* This is where we'll define wrappers for socket IO stuff, for neat winsock compatability */

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "inspircd_config.h"

/* macros to the relevant system address description structs */
#ifdef IPV6

typedef struct sockaddr_in6 insp_sockaddr;
typedef struct in6_addr     insp_inaddr;

#else

typedef struct sockaddr_in  insp_sockaddr;
typedef struct in_addr      insp_inaddr;

#endif

int OpenTCPSocket(); 
bool BindSocket(int sockfd, insp_sockaddr client, insp_sockaddr server, int port, char* addr);
int BindPorts(bool bail);

#endif
