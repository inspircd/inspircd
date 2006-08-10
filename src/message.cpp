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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#include <ext/hash_map>
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "commands.h"
#include "message.h"
#include "inspstring.h"
#include "dns.h"
#include "helperfuncs.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern InspIRCd* ServerInstance;

/* verify that a user's ident and nickname is valid */

int isident(const char* n)
{
	if (!n || !*n)
	{
		return 0;
	}
	for (char* i = (char*)n; *i; i++)
	{
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}
		if (((*i >= '0') && (*i <= '9')) || (*i == '-') || (*i == '.'))
		{
			continue;
		}
		return 0;
	}
	return 1;
}


int isnick(const char* n)
{
	if (!n || !*n)
	{
		return 0;
	}
	int p = 0;
	for (char* i = (char*)n; *i; i++, p++)
	{
		/* "A"-"}" can occur anywhere in a nickname */
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}
		/* "0"-"9", "-" can occur anywhere BUT the first char of a nickname */
		if ((((*i >= '0') && (*i <= '9')) || (*i == '-')) && (i > n))
		{
			continue;
		}
		/* invalid character! abort */
		return 0;
	}
	return (p < NICKMAX - 1);
}

