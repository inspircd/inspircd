/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "servers.h"
#include "inspircd.h"
#include <stdio.h>
#include <map>

serverrec::serverrec()
{
	strlcpy(name,"",256);
	pingtime = 0;
	lastping = time(NULL);
	usercount_i = usercount = opercount = version = 0;
	hops_away = 1;
	signon = time(NULL);
	jupiter = false;
	fd = 0;
	sync_soon = false;
}

 
serverrec::~serverrec()
{
}

serverrec::serverrec(char* n, long ver, bool jupe)
{
	strlcpy(name,n,256);
	lastping = time(NULL);
	usercount_i = usercount = opercount = 0;
	version = ver;
	hops_away = 1;
	signon = time(NULL);
	jupiter = jupe;
	fd = 0;
	sync_soon = false;
}

