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

using namespace std;

#include "inspircd_config.h" 
#include "servers.h"
#include "inspircd.h"
#include <stdio.h>
#include <map>
#include "inspstring.h"
#include "helperfuncs.h"

extern time_t TIME;

serverrec::serverrec()
{
	strlcpy(name,"",256);
	pingtime = 0;
	lastping = TIME;
	usercount_i = usercount = opercount = version = 0;
	hops_away = 1;
	signon = TIME;
	jupiter = false;
	fd = 0;
	sync_soon = false;
	strlcpy(nickserv,"",NICKMAX);
}

 
serverrec::~serverrec()
{
}

serverrec::serverrec(char* n, long ver, bool jupe)
{
	strlcpy(name,n,256);
	lastping = TIME;
	usercount_i = usercount = opercount = 0;
	version = ver;
	hops_away = 1;
	signon = TIME;
	jupiter = jupe;
	fd = 0;
	sync_soon = false;
	strlcpy(nickserv,"",NICKMAX);
}

