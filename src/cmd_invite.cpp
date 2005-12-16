/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain.net>
 *                <Craig.net>
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
#include <string>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "message.h"
#include "commands.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "command_parse.h"
#include "cmd_invite.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;

void cmd_invite::Handle (char **parameters, int pcnt, userrec *user)
{
	if (pcnt == 2)
	{
		userrec* u = Find(parameters[0]);
		chanrec* c = FindChan(parameters[1]);

		if ((!c) || (!u))
		{
			if (!c)
			{
				WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[1]);
			}
			else
			{
				if (c->binarymodes & CM_INVITEONLY)
				{
					WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
				}
			}

			return;
		}

		if (c->binarymodes & CM_INVITEONLY)
		{
			if (cstatus(user,c) < STATUS_HOP)
			{
				WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, c->name);
				return;
			}
		}
		if (has_channel(u,c))
	 	{
	 		WriteServ(user->fd,"443 %s %s %s :Is already on channel %s",user->nick,u->nick,c->name,c->name);
	 		return;
		}
		if (!has_channel(user,c))
	 	{
			WriteServ(user->fd,"442 %s %s :You're not on that channel!",user->nick, c->name);
	  		return;
		}

		int MOD_RESULT = 0;
		FOREACH_RESULT(OnUserPreInvite(user,u,c));
		if (MOD_RESULT == 1) {
			return;
		}

		irc::string xname(c->name);
		u->InviteTo(xname);
		WriteFrom(u->fd,user,"INVITE %s :%s",u->nick,c->name);
		WriteServ(user->fd,"341 %s %s %s",user->nick,u->nick,c->name);
		FOREACH_MOD OnUserInvite(user,u,c);
	}
	else
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InvitedList* il = user->GetInviteList();
		for (InvitedList::iterator i = il->begin(); i != il->end(); i++)
		{
			WriteServ(user->fd,"346 %s :%s",user->nick,i->channel.c_str());
		}
		WriteServ(user->fd,"347 %s :End of INVITE list",user->nick);
	}
}


