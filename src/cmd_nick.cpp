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

#include <string>
#include <vector>
#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "xline.h"
#include "dns.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "commands/cmd_nick.h"

extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;

void cmd_nick::Handle (const char** parameters, int pcnt, userrec *user)
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
		strlcpy(oldnick, user->nick, NICKMAX - 1);
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT)
			return;
		if (user->registered == REG_ALL)
			user->WriteCommon("NICK %s",parameters[0]);
		strlcpy(user->nick, parameters[0], NICKMAX - 1);
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
		return;
	}
	else
	{
		if ((*parameters[0] == ':') && (*(parameters[0]+1) != 0))
		{
			parameters[0]++;
		}
		if (matches_qline(parameters[0]))
		{
			WriteOpers("*** Q-Lined nickname %s from %s!%s@%s: %s",parameters[0],user->nick,user->ident,user->host,matches_qline(parameters[0]));
			user->WriteServ("432 %s %s :Invalid nickname: %s",user->nick,parameters[0],matches_qline(parameters[0]));
			return;
		}
		if ((Find(parameters[0])) && (Find(parameters[0]) != user))
		{
			user->WriteServ("433 %s %s :Nickname is already in use.",user->nick,parameters[0]);
			return;
		}
	}
	if ((isnick(parameters[0]) == 0) && (IS_LOCAL(user)))
	{
		user->WriteServ("432 %s %s :Erroneous Nickname",user->nick,parameters[0]);
		return;
	}

	if (user->registered == REG_ALL)
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT) {
			// if a module returns true, the nick change is silently forbidden.
			return;
		}

		user->WriteCommon("NICK %s",parameters[0]);
		
	}

	strlcpy(oldnick, user->nick, NICKMAX - 1);

	/* change the nick of the user in the users_hash */
	user = user->UpdateNickHash(parameters[0]);
	/* actually change the nick within the record */
	if (!user) return;
	if (!user->nick) return;

	strlcpy(user->nick, parameters[0], NICKMAX - 1);

	log(DEBUG,"new nick set: %s",user->nick);
	
	if (user->registered < REG_NICKUSER)
	{
		user->registered = (user->registered | REG_NICK);
		// dont attempt to look up the dns until they pick a nick... because otherwise their pointer WILL change
		// and unless we're lucky we'll get a duff one later on.
		//user->dns_done = (!lookup_dns(user->nick));
		//if (user->dns_done)
		//	log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);

		if (ServerInstance->Config->NoUserDns)
		{
			user->dns_done = true;
		}
		else
		{
			user->StartDNSLookup();
			if (user->dns_done)
				log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);
		}
	}
	if (user->registered == REG_NICKUSER)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		FOREACH_MOD(I_OnUserRegister,OnUserRegister(user));
		//ConnectUser(user,NULL);
	}
	if (user->registered == REG_ALL)
	{
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
	}
}
