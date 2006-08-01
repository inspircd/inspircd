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

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <string>
#include "users.h"
#include "globals.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "dns.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "socket.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;

bool lookup_dns(const std::string &nick)
{
	return false;
}

void ZapThisDns(int fd)
{
}

void dns_poll(int fdcheck)
{
}
