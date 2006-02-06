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
#include "cmd_who.h"

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
extern userrec* fd_ref_table[MAX_DESCRIPTORS];

void cmd_who::Handle (char **parameters, int pcnt, userrec *user)
{
	chanrec* Ptr = NULL;
	char tmp[10];
	
	/* theres more to do here, but for now just close the socket */
	if (pcnt == 1)
	{
		if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")))
		{
			if ((user->chans.size()) && (user->chans[0].channel))
			{
				int n_list = 0;
			  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					Ptr = i->second->chans[0].channel;
					// suggested by phidjit and FCS
					if ((!common_channels(user,i->second)) && (isnick(i->second->nick)))
					{
						// Bug Fix #29
						*tmp = 0;
						if (*i->second->awaymsg) {
							strlcat(tmp, "G", 9);
						} else {
							strlcat(tmp, "H", 9);
						}
						if (strchr(i->second->modes,'o')) { strlcat(tmp, "*", 9); }
						WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr ? Ptr->name : "*", i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
						if (n_list++ > Config->MaxWhoResults)
						{
							WriteServ(user->fd,"523 %s WHO :Command aborted: More results than configured limit",user->nick);
							break;
						}
					}
				}
			}
			if (Ptr)
			{
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick , parameters[0]);
			}
			else
			{
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
			}
			return;
		}
		if (parameters[0][0] == '#')
		{
			Ptr = FindChan(parameters[0]);
			if (Ptr)
			{
				int n_list = 0;
			  	for (user_hash::const_iterator i = clientlist.begin(); i != clientlist.end(); i++)
				{
					if ((has_channel(i->second,Ptr)) && (isnick(i->second->nick)))
					{
						// Fix Bug #29 - Part 2..
						*tmp = 0;
						if (*i->second->awaymsg) {
							strlcat(tmp, "G", 9);
						} else {
							strlcat(tmp, "H", 9);
						}
						if (strchr(i->second->modes,'o')) { strlcat(tmp, "*", 9); }
						strlcat(tmp, cmode(i->second, Ptr),5);
						WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, Ptr->name, i->second->ident, i->second->dhost, i->second->server, i->second->nick, tmp, i->second->fullname);
                                                n_list++;
                                                if (n_list > Config->MaxWhoResults)
                                                {
                                                        WriteServ(user->fd,"523 %s WHO :Command aborted: More results than configured limit",user->nick);
                                                        break;
                                                }

					}
				}
				WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
			}
			else
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}
		}
		else
		{
			userrec* u = Find(parameters[0]);
			if (u)
			{
				// Bug Fix #29 -- Part 29..
				*tmp = 0;
				if (*u->awaymsg) {
					strlcat(tmp, "G" ,9);
				} else {
					strlcat(tmp, "H" ,9);
				}
				if (strchr(u->modes,'o')) { strlcat(tmp, "*" ,9); }
				WriteServ(user->fd,"352 %s %s %s %s %s %s %s :0 %s",user->nick, u->chans.size() && u->chans[0].channel ? u->chans[0].channel->name
                                : "*", u->ident, u->dhost, u->server, u->nick, tmp, u->fullname);
			}
			WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
		}
	}
	if (pcnt == 2)
	{
                if ((!strcmp(parameters[0],"0")) || (!strcmp(parameters[0],"*")) && (!strcmp(parameters[1],"o")))
                {
		  	for (std::vector<userrec*>::iterator i = all_opers.begin(); i != all_opers.end(); i++)
                        {
				// If i were a rich man.. I wouldn't need to me making these bugfixes..
				// But i'm a poor bastard with nothing better to do.
				userrec* oper = *i;
				*tmp = 0;
				if (*oper->awaymsg) {
					strlcat(tmp, "G" ,9);
				} else {
					strlcat(tmp, "H" ,9);
				}
                                WriteServ(user->fd,"352 %s %s %s %s %s %s %s* :0 %s", user->nick, oper->chans.size() && oper->chans[0].channel ? oper->chans[0].channel->name 
				: "*", oper->ident, oper->dhost, oper->server, oper->nick, tmp, oper->fullname);
                        }
                        WriteServ(user->fd,"315 %s %s :End of /WHO list.",user->nick, parameters[0]);
                        return;
                }
	}
}


