/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
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
#include "inspircd_io.h"
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cmd_list.h"

extern chan_hash chanlist;

void cmd_list::Handle (char **parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
	for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
	{
		// if the channel is not private/secret, OR the user is on the channel anyway
		bool n = i->second->HasUser(user);
		if (((!(i->second->binarymodes & CM_PRIVATE)) && (!(i->second->binarymodes & CM_SECRET))) || (n))
		{
			WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,usercount_i(i->second),chanmodes(i->second,n),i->second->topic);
		}
	}
	WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
}



