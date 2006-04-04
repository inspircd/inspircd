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
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
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
#include "message.h"
#include "commands.h"
#include "mode.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_whois.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

void do_whois(userrec* user, userrec* dest,unsigned long signon, unsigned long idle, char* nick)
{
	// bug found by phidjit - were able to whois an incomplete connection if it had sent a NICK or USER
	if (dest->registered == 7)
	{
		WriteServ(user->fd,"311 %s %s %s %s * :%s",user->nick, dest->nick, dest->ident, dest->dhost, dest->fullname);
		if ((user == dest) || (*user->oper))
		{
			WriteServ(user->fd,"378 %s %s :is connecting from *@%s %s",user->nick, dest->nick, dest->host, inet_ntoa(dest->ip4));
		}
		std::string cl = chlist(dest,user);
		if (cl.length())
		{
			if (cl.length() > 400)
			{
				split_chlist(user,dest,cl);
			}
			else
			{
				WriteServ(user->fd,"319 %s %s :%s",user->nick, dest->nick, cl.c_str());
			}
		}
		if (*Config->HideWhoisServer && !(*user->oper))
		{
			WriteServ(user->fd,"312 %s %s %s :%s",user->nick, dest->nick, Config->HideWhoisServer, Config->Network);
		}
		else
		{
			WriteServ(user->fd,"312 %s %s %s :%s",user->nick, dest->nick, dest->server, GetServerDescription(dest->server).c_str());
		}
		if (*dest->awaymsg)
		{
			WriteServ(user->fd,"301 %s %s :%s",user->nick, dest->nick, dest->awaymsg);
		}
		if (*dest->oper)
		{
			WriteServ(user->fd,"313 %s %s :is %s %s on %s",user->nick, dest->nick, (strchr("aeiou",*dest->oper) ? "an" : "a"),dest->oper, Config->Network);
		}
		if ((!signon) && (!idle))
		{
			FOREACH_MOD(I_OnWhois,OnWhois(user,dest));
		}
		if (!strcasecmp(user->server,dest->server))
		{
			// idle time and signon line can only be sent if youre on the same server (according to RFC)
			WriteServ(user->fd,"317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, abs((dest->idle_lastmsg)-TIME), dest->signon);
		}
		else
		{
			if ((idle) || (signon))
				WriteServ(user->fd,"317 %s %s %d %d :seconds idle, signon time",user->nick, dest->nick, idle, signon);
		}
		WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, dest->nick);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, nick);
		WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, nick);
	}
}

void cmd_whois::Handle (char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-1,0))
		return;
	dest = Find(parameters[0]);
	if (dest)
	{
		do_whois(user,dest,0,0,parameters[0]);
	}
        else
        {
                /* no such nick/channel */
                WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
                WriteServ(user->fd,"318 %s %s :End of /WHOIS list.",user->nick, parameters[0]);
	}
}
