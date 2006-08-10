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

#include <vector>
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands.h"
#include "helperfuncs.h"
#include "message.h"
#include "commands/cmd_invite.h"

extern InspIRCd* ServerInstance;
extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

void cmd_invite::Handle (const char** parameters, int pcnt, userrec *user)
{
	int MOD_RESULT = 0;

	if (pcnt == 2)
	{
		userrec* u = ServerInstance->FindNick(parameters[0]);
		chanrec* c = ServerInstance->FindChan(parameters[1]);

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

			return;
		}

		if ((c->modes[CM_INVITEONLY]) && (IS_LOCAL(user)))
		{
			if (c->GetStatus(user) < STATUS_HOP)
			{
				user->WriteServ("482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, c->name);
				return;
			}
		}

		if (c->HasUser(u))
	 	{
	 		user->WriteServ("443 %s %s %s :Is already on channel %s",user->nick,u->nick,c->name,c->name);
	 		return;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
	 	{
			user->WriteServ("442 %s %s :You're not on that channel!",user->nick, c->name);
	  		return;
		}

		FOREACH_RESULT(I_OnUserPreInvite,OnUserPreInvite(user,u,c));

		if (MOD_RESULT == 1)
		{
			return;
		}

		irc::string xname(c->name);
		u->InviteTo(xname);
		u->WriteFrom(user,"INVITE %s :%s",u->nick,c->name);
		user->WriteServ("341 %s %s %s",user->nick,u->nick,c->name);
		FOREACH_MOD(I_OnUserInvite,OnUserInvite(user,u,c));
	}
	else
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InvitedList* il = user->GetInviteList();
		for (InvitedList::iterator i = il->begin(); i != il->end(); i++)
		{
			user->WriteServ("346 %s :%s",user->nick,i->channel.c_str());
		}
		user->WriteServ("347 %s :End of INVITE list",user->nick);
	}
}
