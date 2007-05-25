/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "xline.h"
#include "commands/cmd_nick.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_nick(Instance);
}

/** Handle nick changes from users.
 * NOTE: If you are used to ircds based on ircd2.8, and are looking
 * for the client introduction code in here, youre in the wrong place.
 * You need to look in the spanningtree module for this!
 */
CmdResult cmd_nick::Handle (const char** parameters, int pcnt, userrec *user)
{
	char oldnick[NICKMAX];

	if (!*parameters[0] || !*user->nick)
	{
		/* We cant put blanks in the parameters, so for this (extremely rare) issue we just put '*' here. */
		user->WriteServ("432 %s * :Erroneous Nickname", *user->nick ? user->nick : "*");
		return CMD_FAILURE;
	}

	if (irc::string(user->nick) == irc::string(parameters[0]))
	{
		/* If its exactly the same, even case, dont do anything. */
		if (!strcmp(user->nick,parameters[0]))
			return CMD_SUCCESS;

		/* Its a change of case. People insisted that they should be
		 * able to do silly things like this even though the RFC says
		 * the nick AAA is the same as the nick aaa.
		 */
		strlcpy(oldnick, user->nick, NICKMAX - 1);
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT)
			return CMD_FAILURE;
		if (user->registered == REG_ALL)
			user->WriteCommon("NICK %s",parameters[0]);
		strlcpy(user->nick, parameters[0], NICKMAX - 1);
		user->InvalidateCache();
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
		/* Check for nickname overruled -
		 * This happens when one user has connected and sent only NICK, and is essentially
		 * "camping" upon a nickname. To give the new user connecting a fair chance of having
		 * the nickname too, we force a nickchange on the older user (Simply the one who was
		 * here first, no TS checks need to take place here)
		 */
		userrec* InUse = ServerInstance->FindNick(parameters[0]);
		if (InUse && (InUse != user) && (ServerInstance->IsNick(parameters[0])))
		{
			if (InUse->registered != REG_ALL)
			{
				/* change the nick of the older user to nnn-overruled,
				 * where nnn is their file descriptor. We know this to be unique.
				 * NOTE: We must do this and not quit the user, even though we do
				 * not have UID support yet. This is because if we set this user
				 * as quitting and then introduce the new user before the old one
				 * has quit, then the user hash gets totally buggered.
				 * (Yes, that is a technical term). -- Brain
				 */
				std::string changeback = ConvToStr(InUse->GetFd()) + "-overruled";
				InUse->WriteTo(InUse, "NICK %s", changeback.c_str());
				InUse->WriteServ("433 %s %s :Nickname overruled.", InUse->nick, InUse->nick);
				InUse->UpdateNickHash(changeback.c_str());
				strlcpy(InUse->nick, changeback.c_str(), NICKMAX - 1);
				InUse->InvalidateCache();
				/* Take away their nickname-sent state forcing them to send a nick again */
				InUse->registered &= ~REG_NICK;
			}
			else
			{
				user->WriteServ("433 %s %s :Nickname is already in use.", user->registered >= REG_NICK ? user->nick : "*", parameters[0]);
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

	user->InvalidateCache();

	/* Update display nicks */
	for (UCListIter v = user->chans.begin(); v != user->chans.end(); v++)
	{
		CUList* ulist = v->first->GetUsers();
		CUList::iterator i = ulist->find(user);
		if (i != ulist->end())
			i->second = user->nick;
	}

	if (user->registered < REG_NICKUSER)
	{
		user->registered = (user->registered | REG_NICK);

		if (ServerInstance->Config->NoUserDns)
		{
			user->dns_done = true;
			ServerInstance->next_call = ServerInstance->Time();
		}
		else
		{
			user->StartDNSLookup();
			if (user->dns_done)
			{
				/* Cached result or instant failure - fall right through if possible */
				ServerInstance->next_call = ServerInstance->Time();
			}
			else
			{
				if (ServerInstance->next_call > ServerInstance->Time() + ServerInstance->Config->dns_timeout)
					ServerInstance->next_call = ServerInstance->Time() + ServerInstance->Config->dns_timeout;
			}
		}
	}
	if (user->registered == REG_NICKUSER)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserRegister,OnUserRegister(user));
		if (MOD_RESULT > 0)
			return CMD_FAILURE;
	}
	if (user->registered == REG_ALL)
	{
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
	}

	return CMD_SUCCESS;

}

