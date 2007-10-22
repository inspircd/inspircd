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
#include "commands/cmd_invite.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandInvite(Instance);
}

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle (const char** parameters, int pcnt, User *user)
{
	int MOD_RESULT = 0;

	if (pcnt == 2)
	{
		User* u = ServerInstance->FindNick(parameters[0]);
		Channel* c = ServerInstance->FindChan(parameters[1]);

		if ((!c) || (!u))
		{
			if (!c)
			{
				user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[1]);
			}
			else
			{
				user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
			}

			return CMD_FAILURE;
		}

		if ((c->IsModeSet('i')) && (IS_LOCAL(user)))
		{
			if (c->GetStatus(user) < STATUS_HOP)
			{
				user->WriteServ("482 %s %s :You must be a channel %soperator", user->nick, c->name, c->GetStatus(u) == STATUS_HOP ? "" : "half-");
				return CMD_FAILURE;
			}
		}

		if (c->HasUser(u))
	 	{
	 		user->WriteServ("443 %s %s %s :is already on channel",user->nick,u->nick,c->name);
	 		return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
	 	{
			user->WriteServ("442 %s %s :You're not on that channel!",user->nick, c->name);
	  		return CMD_FAILURE;
		}

		FOREACH_RESULT(I_OnUserPreInvite,OnUserPreInvite(user,u,c));

		if (MOD_RESULT == 1)
		{
			return CMD_FAILURE;
		}

		u->InviteTo(c->name);
		u->WriteFrom(user,"INVITE %s :%s",u->nick,c->name);
		user->WriteServ("341 %s %s %s",user->nick,u->nick,c->name);
		switch (ServerInstance->Config->AnnounceInvites)
		{
			case ServerConfig::INVITE_ANNOUNCE_ALL:
				c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s invited %s into the channel", c->name, user->nick, u->nick);
			break;
			case ServerConfig::INVITE_ANNOUNCE_OPS:
				c->WriteAllExceptSender(user, true, '@', "NOTICE %s :*** %s invited %s into the channel", c->name, user->nick, u->nick);
			break;
			case ServerConfig::INVITE_ANNOUNCE_DYNAMIC:
				if (c->IsModeSet('i'))
					c->WriteAllExceptSender(user, true, '@', "NOTICE %s :*** %s invited %s into the channel", c->name, user->nick, u->nick);
				else
					c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s invited %s into the channel", c->name, user->nick, u->nick);
			break;
			default:
				/* Nobody */
			break;
		}
		FOREACH_MOD(I_OnUserInvite,OnUserInvite(user,u,c));
	}
	else
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InvitedList* il = user->GetInviteList();
		for (InvitedList::iterator i = il->begin(); i != il->end(); i++)
		{
			user->WriteServ("346 %s :%s",user->nick,i->c_str());
		}
		user->WriteServ("347 %s :End of INVITE list",user->nick);
	}
	return CMD_SUCCESS;
}

