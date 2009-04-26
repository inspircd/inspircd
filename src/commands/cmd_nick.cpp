/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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
	std::string oldnick;

	if (parameters[0].empty())
	{
		/* We cant put blanks in the parameters, so for this (extremely rare) issue we just put '*' here. */
		user->WriteNumeric(432, "%s * :Erroneous Nickname", user->nick.empty() ? user->nick.c_str() : "*");
		return CMD_FAILURE;
	}

	if (((!ServerInstance->IsNick(parameters[0].c_str(), ServerInstance->Config->Limits.NickMax))) && (IS_LOCAL(user)))
	{
		if (!allowinvalid)
		{
			if (parameters[0] == "0")
			{
				// Special case, Fake a /nick UIDHERE. Useful for evading "ERR: NICK IN USE" on connect etc.
				std::vector<std::string> p2;
				std::deque<classbase*> dummy;
				p2.push_back(user->uuid);
				this->HandleInternal(1, dummy);
				this->Handle(p2, user);
				this->HandleInternal(0, dummy);
				return CMD_SUCCESS;
			}

			user->WriteNumeric(432, "%s %s :Erroneous Nickname", user->nick.c_str(),parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	if (assign(user->nick) == parameters[0])
	{
		/* If its exactly the same, even case, dont do anything. */
		if (parameters[0] == user->nick)
		{
			return CMD_SUCCESS;
		}

		/* Its a change of case. People insisted that they should be
		 * able to do silly things like this even though the RFC says
		 * the nick AAA is the same as the nick aaa.
		 */
		oldnick.assign(user->nick, 0, IS_LOCAL(user) ? ServerInstance->Config->Limits.NickMax : MAXBUF);
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user,parameters[0]));
		if (MOD_RESULT)
			return CMD_FAILURE;
		if (user->registered == REG_ALL)
			user->WriteCommon("NICK %s",parameters[0].c_str());
		user->nick.assign(parameters[0], 0, IS_LOCAL(user) ? ServerInstance->Config->Limits.NickMax : MAXBUF);
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
				if (user->registered == REG_ALL)
				{
					ServerInstance->SNO->WriteToSnoMask('x', "Q-Lined nickname %s from %s!%s@%s: %s", parameters[0].c_str(), user->nick.c_str(), user->ident.c_str(), user->host.c_str(), mq->reason);
				}
				user->WriteNumeric(432, "%s %s :Invalid nickname: %s",user->nick.c_str(), parameters[0].c_str(), mq->reason);
				return CMD_FAILURE;
			}

			if (ServerInstance->Config->RestrictBannedUsers)
			{
				for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
				{
					Channel *chan = i->first;
					if (chan->GetStatus(user) < STATUS_VOICE && chan->IsBanned(user))
					{
						user->WriteNumeric(404, "%s %s :Cannot send to channel (you're banned)", user->nick.c_str(), chan->name.c_str());
						return CMD_FAILURE;
					}
				}
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
		if (InUse && (InUse != user))
		{
			if (InUse->registered != REG_ALL)
			{
				/* force the camper to their UUID, and ask them to re-send a NICK. */
				InUse->WriteTo(InUse, "NICK %s", InUse->uuid.c_str());
				InUse->WriteNumeric(433, "%s %s :Nickname overruled.", InUse->nick.c_str(), InUse->nick.c_str());
				InUse->UpdateNickHash(InUse->uuid.c_str());
				InUse->nick.assign(InUse->uuid, 0, IS_LOCAL(InUse) ? ServerInstance->Config->Limits.NickMax : MAXBUF);
				InUse->InvalidateCache();
				InUse->registered &= ~REG_NICK;
			}
			else
			{
				/* No camping, tell the incoming user  to stop trying to change nick ;p */
				user->WriteNumeric(433, "%s %s :Nickname is already in use.", user->registered >= REG_NICK ? user->nick.c_str() : "*", parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
	}


	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnUserPreNick,OnUserPreNick(user, parameters[0]));
	if (MOD_RESULT)
		// if a module returns true, the nick change is silently forbidden.
		return CMD_FAILURE;

	if (user->registered == REG_ALL)
		user->WriteCommon("NICK %s", parameters[0].c_str());

	oldnick.assign(user->nick, 0, IS_LOCAL(user) ? ServerInstance->Config->Limits.NickMax : MAXBUF);

	/* change the nick of the user in the users_hash */
	user = user->UpdateNickHash(parameters[0].c_str());

	/* actually change the nick within the record */
	if (!user)
		return CMD_FAILURE;

	user->nick.assign(parameters[0], 0, IS_LOCAL(user) ? ServerInstance->Config->Limits.NickMax : MAXBUF);
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
		if (user->registered == REG_NICKUSER)
		{
			/* user is registered now, bit 0 = USER command, bit 1 = sent a NICK command */
			MOD_RESULT = 0;
			FOREACH_RESULT(I_OnUserRegister,OnUserRegister(user));
			if (MOD_RESULT > 0)
				return CMD_FAILURE;

			// return early to not penalize new users
			return CMD_SUCCESS;
		}
	}

	if (user->registered == REG_ALL)
	{
		user->IncreasePenalty(10);
		FOREACH_MOD(I_OnUserPostNick,OnUserPostNick(user, oldnick));
	}

	return CMD_SUCCESS;

}

CmdResult CommandNick::HandleInternal(const unsigned int id, const std::deque<classbase*>&)
{
	allowinvalid = (id != 0);
	return CMD_SUCCESS;
}

