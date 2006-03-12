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
#include "cmd_topic.h"

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

void cmd_topic::Handle (char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;

	if (pcnt == 1)
	{
		Ptr = FindChan(parameters[0]);
		if (Ptr)
		{
			if ((Ptr->modes[CM_SECRET]) && (!Ptr->HasUser(user)))
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, Ptr->name);
				return;
			}
			if (Ptr->topicset)
			{
				WriteServ(user->fd,"332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
				WriteServ(user->fd,"333 %s %s %s %d", user->nick, Ptr->name, Ptr->setby, Ptr->topicset);
			}
			else
			{
				WriteServ(user->fd,"331 %s %s :No topic is set.", user->nick, Ptr->name);
			}
		}
		else
		{
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
		return;
	}
	else if (pcnt>1)
	{
		Ptr = FindChan(parameters[0]);
		if (Ptr)
		{
			if (IS_LOCAL(user))
			{
				if (!Ptr->HasUser(user))
				{
					WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, Ptr->name);
					return;
				}
				if ((Ptr->modes[CM_TOPICLOCK]) && (cstatus(user,Ptr)<STATUS_HOP))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel", user->nick, Ptr->name);
					return;
				}
			}
			char topic[MAXTOPIC];
			strlcpy(topic,parameters[1],MAXTOPIC-1);

                        if (IS_LOCAL(user))
                        {
                                int MOD_RESULT = 0;
                                FOREACH_RESULT(I_OnLocalTopicChange,OnLocalTopicChange(user,Ptr,topic));
                                if (MOD_RESULT)
                                        return;
                        }

			strlcpy(Ptr->topic,topic,MAXTOPIC-1);
			strlcpy(Ptr->setby,user->nick,NICKMAX-1);
			Ptr->topicset = TIME;
			WriteChannel(Ptr,user,"TOPIC %s :%s",Ptr->name, Ptr->topic);
			if (IS_LOCAL(user))
			{
				FOREACH_MOD(I_OnPostLocalTopicChange,OnPostLocalTopicChange(user,Ptr,topic));
			}
		}
		else
		{
			WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
		}
	}
}


