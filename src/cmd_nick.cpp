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

#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "inspircd.h"
#include "xline.h"
#include "commands/cmd_nick.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_nick(Instance);
}

CmdResult cmd_nick::Handle (const char** parameters, int pcnt, userrec *user)
{
	char oldnick[NICKMAX];

	if (!parameters[0][0])
	{
		ServerInstance->Log(DEBUG,"zero length new nick passed to handle_nick");
		return CMD_FAILURE;
	}
	if (!*user->nick)
	{
		ServerInstance->Log(DEBUG,"invalid old nick passed to handle_nick");
		return CMD_FAILURE;
	}
	ServerInstance->Log(DEBUG,"Fall through");
	if (irc::string(user->nick) == irc::string(parameters[0]))
	{
		/* If its exactly the same, even case, dont do anything. */
		if (!strcmp(user->nick,parameters[0]))
			return CMD_SUCCESS;

		/* Its a change of case. People insisted that they should be
		 * able to do silly things like this even though the RFC says
		 * the nick AAA is the same as the nick aaa.
		 */
		ServerInstance->Log(DEBUG,"old nick is new nick, not updating hash (case change only)");
		strlcpy(oldnick, user->nick, NICKMAX - 1);
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT)
			return CMD_FAILURE;
		if (user->registered == REG_ALL)
			user->WriteCommon("NICK %s",parameters[0]);
		strlcpy(user->nick, parameters[0], NICKMAX - 1);
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
		return CMD_SUCCESS;
	}
	else
	{
		QLine* mq = ServerInstance->XLines->matches_qline(parameters[0]);
		if (mq)
		{
			ServerInstance->SNO->WriteToSnoMask('x', "Q-Lined nickname %s from %s!%s@%s: %s", parameters[0], user->nick, user->ident, user->host, mq->reason);
			user->WriteServ("432 %s %s :Invalid nickname: %s",user->nick,parameters[0], mq->reason);
			return CMD_FAILURE;
		}
		if ((ServerInstance->FindNick(parameters[0])) && (ServerInstance->FindNick(parameters[0]) != user) && (ServerInstance->IsNick(parameters[0])))
		{
			userrec* InUse = ServerInstance->FindNick(parameters[0]);
			if (InUse->registered != REG_ALL)
			{
				userrec::QuitUser(ServerInstance, InUse, "Nickname overruled");
			}
			else
			{
				user->WriteServ("433 %s %s :Nickname is already in use.",user->nick,parameters[0]);
				return CMD_FAILURE;
			}
		}
	}
	if ((!ServerInstance->IsNick(parameters[0])) && (IS_LOCAL(user)))
	{
		user->WriteServ("432 %s %s :Erroneous Nickname",user->nick,parameters[0]);
		return CMD_FAILURE;
	}

	if (user->registered == REG_ALL)
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT) {
			// if a module returns true, the nick change is silently forbidden.
			return CMD_FAILURE;
		}

		user->WriteCommon("NICK %s",parameters[0]);
		
	}

	strlcpy(oldnick, user->nick, NICKMAX - 1);

	/* change the nick of the user in the users_hash */
	user = user->UpdateNickHash(parameters[0]);
	/* actually change the nick within the record */
	if (!user) return CMD_FAILURE;
	if (!*user->nick) return CMD_FAILURE;

	strlcpy(user->nick, parameters[0], NICKMAX - 1);

	ServerInstance->Log(DEBUG,"new nick set: %s",user->nick);
	
	if (user->registered < REG_NICKUSER)
	{
		user->registered = (user->registered | REG_NICK);
		// dont attempt to look up the dns until they pick a nick... because otherwise their pointer WILL change
		// and unless we're lucky we'll get a duff one later on.
		//user->dns_done = (!lookup_dns(user->nick));
		//if (user->dns_done)
		//	ServerInstance->Log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);

		if (ServerInstance->Config->NoUserDns)
		{
			user->dns_done = true;
		}
		else
		{
			user->StartDNSLookup();
			if (user->dns_done)
				ServerInstance->Log(DEBUG,"Aborting dns lookup of %s because dns server experienced a failure.",user->nick);
		}

		ServerInstance->next_call = ServerInstance->Time() + ServerInstance->Config->dns_timeout;
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

	return CMD_SUCCESS;

}

