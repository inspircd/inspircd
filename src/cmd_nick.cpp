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
#include "dns.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_nick.h"

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

void cmd_nick::Handle (char **parameters, int pcnt, userrec *user)
{
	char oldnick[NICKMAX];

	if (pcnt < 1) 
	{
		log(DEBUG,"not enough params for handle_nick");
		return;
	}
	if (!parameters[0])
	{
		log(DEBUG,"invalid parameter passed to handle_nick");
		return;
	}
	if (!parameters[0][0])
	{
		log(DEBUG,"zero length new nick passed to handle_nick");
		return;
	}
	if (!user)
	{
		log(DEBUG,"invalid user passed to handle_nick");
		return;
	}
	if (!user->nick)
	{
		log(DEBUG,"invalid old nick passed to handle_nick");
		return;
	}
	if (irc::string(user->nick) == irc::string(parameters[0]))
	{
		/* If its exactly the same, even case, dont do anything. */
		if (!strcmp(user->nick,parameters[0]))
			return;
		/* Its a change of case. People insisted that they should be
		 * able to do silly things like this even though the RFC says
		 * the nick AAA is the same as the nick aaa.
		 */
		log(DEBUG,"old nick is new nick, not updating hash (case change only)");
		strlcpy(oldnick,user->nick,NICKMAX);
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT)
			return;
		strlcpy(user->nick,parameters[0],NICKMAX);
		if (user->registered == 7)
			WriteCommon(user,"NICK %s",parameters[0]);
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
		return;
	}
	else
	{
		if (strlen(parameters[0]) > 1)
		{
			if (parameters[0][0] == ':')
			{
				*parameters[0]++;
			}
		}
		if (matches_qline(parameters[0]))
		{
			WriteOpers("*** Q-Lined nickname %s from %s!%s@%s: %s",parameters[0],user->nick,user->ident,user->host,matches_qline(parameters[0]));
			WriteServ(user->fd,"432 %s %s :Invalid nickname: %s",user->nick,parameters[0],matches_qline(parameters[0]));
			return;
		}
		if ((Find(parameters[0])) && (Find(parameters[0]) != user))
		{
			WriteServ(user->fd,"433 %s %s :Nickname is already in use.",user->nick,parameters[0]);
			return;
		}
	}
	if (isnick(parameters[0]) == 0)
	{
		WriteServ(user->fd,"432 %s %s :Erroneous Nickname",user->nick,parameters[0]);
		return;
	}

	if (user->registered == 7)
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT) {
			// if a module returns true, the nick change is silently forbidden.
			return;
		}

		WriteCommon(user,"NICK %s",parameters[0]);
		
	}

	strlcpy(oldnick,user->nick,NICKMAX);

	/* change the nick of the user in the users_hash */
	user = ReHashNick(user->nick, parameters[0]);
	/* actually change the nick within the record */
	if (!user) return;
	if (!user->nick) return;

	strlcpy(user->nick, parameters[0],NICKMAX);

	log(DEBUG,"new nick set: %s",user->nick);
	
	if (user->registered < 3)
	{
		user->registered = (user->registered | 2);
		// dont attempt to look up the dns until they pick a nick... because otherwise their pointer WILL change
		// and unless we're lucky we'll get a duff one later on.
		//user->dns_done = (!lookup_dns(user->nick));
		//if (user->dns_done)
		//	log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);

#ifdef THREADED_DNS
		// initialize their dns lookup thread
		if (pthread_create(&user->dnsthread, NULL, dns_task, (void *)user) != 0)
		{
			log(DEBUG,"Failed to create DNS lookup thread for user %s",user->nick);
		}
#else
		user->dns_done = (!lookup_dns(user->nick));
		if (user->dns_done)
			log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);
#endif
	
	}
	if (user->registered == 3)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD(I_OnUserRegister,OnUserRegister(user));
		//ConnectUser(user,NULL);
	}
	if (user->registered == 7)
	{
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
	}
}

