/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain.net>
 *                <Craig.net>
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
#include "cmd_privmsg.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern std::vector<userrec*> all_opers;
extern std::vector<userrec*> local_users;
extern userrec* fd_ref_table[65536];

void cmd_privmsg::Handle (char **parameters, int pcnt, userrec *user)
{
	userrec *dest;
	chanrec *chan;

	user->idle_lastmsg = TIME;
	
	/*if (ServerInstance->Parser->LoopCall(handle_privmsg,parameters,pcnt,user,0,pcnt-2,0))
		return;*/
        if (parameters[0][0] == '$')
	{
		// notice to server mask
		char* servermask = parameters[0];
		servermask++;
		if (match(Config->ServerName,servermask))
                {
			ServerPrivmsgAll("%s",parameters[1]);
                }
		return;
        }
        else if (parameters[0][0] == '#')
	{
		chan = FindChan(parameters[0]);
		if (chan)
		{
			if ((chan->binarymodes & CM_NOEXTERNAL) && (!has_channel(user,chan)))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (no external messages)", user->nick, chan->name);
				return;
			}
			if ((chan->binarymodes & CM_MODERATED) && (cstatus(user,chan)<STATUS_VOICE))
			{
				WriteServ(user->fd,"404 %s %s :Cannot send to channel (+m)", user->nick, chan->name);
				return;
			}
			
			int MOD_RESULT = 0;

			std::string temp = parameters[1];
			FOREACH_RESULT(OnUserPreMessage(user,chan,TYPE_CHANNEL,temp));
			if (MOD_RESULT) {
				return;
			}
			parameters[1] = (char*)temp.c_str();

			if (temp == "")
			{
				WriteServ(user->fd,"412 %s No text to send", user->nick);
				return;
			}
			
			ChanExceptSender(chan, user, "PRIVMSG %s :%s", chan->name, parameters[1]);
			FOREACH_MOD OnUserMessage(user,chan,TYPE_CHANNEL,parameters[1]);
		}
		else
		{
			/* no such nick/channel */
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
		return;
	}

	dest = Find(parameters[0]);
	if (dest)
	{
		if (strcmp(dest->awaymsg,""))
		{
			/* auto respond with aweh msg */
			WriteServ(user->fd,"301 %s %s :%s",user->nick,dest->nick,dest->awaymsg);
		}

		int MOD_RESULT = 0;
		
		std::string temp = parameters[1];
		FOREACH_RESULT(OnUserPreMessage(user,dest,TYPE_USER,temp));
		if (MOD_RESULT) {
			return;
		}
		parameters[1] = (char*)temp.c_str();

		if (dest->fd > -1)
		{
			// direct write, same server
			WriteTo(user, dest, "PRIVMSG %s :%s", dest->nick, parameters[1]);
		}

		FOREACH_MOD OnUserMessage(user,dest,TYPE_USER,parameters[1]);
	}
	else
	{
		/* no such nick/channel */
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}


