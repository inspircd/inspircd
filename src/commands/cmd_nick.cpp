/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "commands/cmd_nick.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandNick(Instance);
}

/** Handle nick changes from users.
 * NOTE: If you are used to ircds based on ircd2.8, and are looking
 * for the client introduction code in here, youre in the wrong place.
 * You need to look in the spanningtree module for this!
 */
CmdResult CommandNick::Handle (const std::vector<std::string>& parameters, User *user)
{
	char oldnick[NICKMAX];

	if (parameters[0].empty())
	{
		/* We cant put blanks in the parameters, so for this (extremely rare) issue we just put '*' here. */
		user->WriteNumeric(432, "%s * :Erroneous Nickname", *user->nick ? user->nick : "*");
		return CMD_FAILURE;
	}

	if (irc::string(user->nick) == assign(parameters[0]))
	{
		/* If its exactly the same, even case, dont do anything. */
		if (parameters[0] == user->nick)
			return CMD_SUCCESS;

		/* Its a change of case. People insisted that they should be
		 * able to do silly things like this even though the RFC says
		 * the nick AAA is the same as the nick aaa.
		 */
		strlcpy(oldnick, user->nick, NICKMAX - 1);
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0].c_str()));
		if (MOD_RESULT)
			return CMD_FAILURE;
		if (user->registered == REG_ALL)
			user->WriteCommon("NICK %s",parameters[0].c_str());
		strlcpy(user->nick, parameters[0].c_str(), NICKMAX - 1);
		user->InvalidateCache();
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
		return CMD_SUCCESS;
	}
	else
	{
		/*
		 * Don't check Q:Lines if it's a server-enforced change, just on the off-chance some fucking *moron*
		 * tries to Q:Line SIDs, also, this means we just get our way period, as it really should be.
		 * Thanks Kein for finding this. -- w00t
		 *
		 * Also don't check Q:Lines for remote nickchanges, they should have our Q:Lines anyway to enforce themselves.
		 *		-- w00t
		 */
		if (!allowinvalid || !IS_LOCAL(user))
		{
			XLine* mq = ServerInstance->XLines->MatchesLine("Q",parameters[0]);
			if (mq)
			{
				ServerInstance->SNO->WriteToSnoMask('x', "Q-Lined nickname %s from %s!%s@%s: %s", parameters[0].c_str(), user->nick, user->ident, user->host, mq->reason);
				user->WriteNumeric(432, "%s %s :Invalid nickname: %s",user->nick, parameters[0].c_str(), mq->reason);
				return CMD_FAILURE;
			}
		}

		/*
		 * Uh oh.. if the nickname is in use, and it's not in use by the person using it (doh) --
		 * then we have a potential collide. Check whether someone else is camping on the nick
		 * (i.e. connect -> send NICK, don't send USER.) If they are camping, force-change the
		 * camper to their UID, and allow the incoming nick change.
		 *
		 * If the guy using the nick is already using it, tell the incoming nick change to gtfo,
		 * because the nick is already (rightfully) in use. -- w00t
		 */
		User* InUse = ServerInstance->FindNickOnly(parameters[0]);
		if (InUse && (InUse != user) && ((ServerInstance->IsNick(parameters[0].c_str()) || allowinvalid)))
		{
			if (InUse->registered != REG_ALL)
			{
				/* force the camper to their UUID, and ask them to re-send a NICK. */
				InUse->WriteTo(InUse, "NICK %s", InUse->uuid);
				InUse->WriteNumeric(433, "%s %s :Nickname overruled.", InUse->nick, InUse->nick);
				InUse->UpdateNickHash(InUse->uuid);
				strlcpy(InUse->nick, InUse->uuid, NICKMAX - 1);
				InUse->InvalidateCache();
				InUse->registered &= ~REG_NICK;
			}
			else
			{
				/* No camping, tell the incoming user  to stop trying to change nick ;p */
				user->WriteNumeric(433, "%s %s :Nickname is already in use.", user->registered >= REG_NICK ? user->nick : "*", parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
	}
	if (((!ServerInstance->IsNick(parameters[0].c_str()))) && (IS_LOCAL(user)))
	{
		if (!allowinvalid)
		{
			user->WriteNumeric(432, "%s %s :Erroneous Nickname", user->nick,parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user, parameters[0]));
	if (MOD_RESULT)
		// if a module returns true, the nick change is silently forbidden.
		return CMD_FAILURE;

	if (user->registered == REG_ALL)
		user->WriteCommon("NICK %s", parameters[0].c_str());

	strlcpy(oldnick, user->nick, NICKMAX - 1);

	/* change the nick of the user in the users_hash */
	user = user->UpdateNickHash(parameters[0].c_str());

	/* actually change the nick within the record */
	if (!user)
		return CMD_FAILURE;
	if (!*user->nick)
		return CMD_FAILURE;

	strlcpy(user->nick, parameters[0].c_str(), NICKMAX - 1);

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
	}
	if (user->registered == REG_NICKUSER)
	{
		/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
		MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserRegister,OnUserRegister(user));
		if (MOD_RESULT > 0)
			return CMD_FAILURE;
	}
	if (user->registered == REG_ALL)
	{
		user->IncreasePenalty(10);
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user,oldnick));
	}

	return CMD_SUCCESS;

}

CmdResult CommandNick::HandleInternal(const unsigned int id, const std::deque<classbase*>&)
{
	allowinvalid = (id != 0);
	return CMD_SUCCESS;
}

