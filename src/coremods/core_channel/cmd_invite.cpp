/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "core_channel.h"
#include "invite.h"

CommandInvite::CommandInvite(Module* parent, Invite::APIImpl& invapiimpl)
	: Command(parent, "INVITE", 0, 0)
	, invapi(invapiimpl)
{
	Penalty = 4;
	syntax = "[<nick> <channel> [<time>]]";
}

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle(User* user, const Params& parameters)
{
	ModResult MOD_RESULT;

	if (parameters.size() >= 2)
	{
		User* u;
		if (IS_LOCAL(user))
			u = ServerInstance->FindNickOnly(parameters[0]);
		else
			u = ServerInstance->FindNick(parameters[0]);

		Channel* c = ServerInstance->FindChan(parameters[1]);
		time_t timeout = 0;
		if (parameters.size() >= 3)
		{
			if (IS_LOCAL(user))
			{
				unsigned long duration;
				if (!InspIRCd::Duration(parameters[2], duration))
				{
					user->WriteNotice("*** Invalid duration for invite");
					return CMD_FAILURE;
				}
				timeout = ServerInstance->Time() + duration;
			}
			else if (parameters.size() > 3)
				timeout = ConvToNum<time_t>(parameters[3]);
		}

		if (!c)
		{
			user->WriteNumeric(Numerics::NoSuchChannel(parameters[1]));
			return CMD_FAILURE;
		}
		if ((!u) || (u->registered != REG_ALL))
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CMD_FAILURE;
		}

		// Verify channel timestamp if the INVITE is coming from a remote server
		if (!IS_LOCAL(user))
		{
			// Remote INVITE commands must carry a channel timestamp
			if (parameters.size() < 3)
				return CMD_INVALID;

			// Drop the invite if our channel TS is lower
			time_t RemoteTS = ConvToNum<time_t>(parameters[2]);
			if (c->age < RemoteTS)
				return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, c->name, "You're not on that channel!");
			return CMD_FAILURE;
		}

		if (c->HasUser(u))
		{
			user->WriteNumeric(ERR_USERONCHANNEL, u->nick, c->name, "is already on channel");
			return CMD_FAILURE;
		}

		FIRST_MOD_RESULT(OnUserPreInvite, MOD_RESULT, (user,u,c,timeout));

		if (MOD_RESULT == MOD_RES_DENY)
		{
			return CMD_FAILURE;
		}
		else if (MOD_RESULT == MOD_RES_PASSTHRU)
		{
			if (IS_LOCAL(user))
			{
				unsigned int rank = c->GetPrefixValue(user);
				if (rank < HALFOP_VALUE)
				{
					// Check whether halfop mode is available and phrase error message accordingly
					ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, c->name, InspIRCd::Format("You must be a channel %soperator",
						(mh && mh->name == "halfop" ? "half-" : "")));
					return CMD_FAILURE;
				}
			}
		}

		LocalUser* const localtargetuser = IS_LOCAL(u);
		if (localtargetuser)
		{
			invapi.Create(localtargetuser, c, timeout);
			ClientProtocol::Messages::Invite invitemsg(user, localtargetuser, c);
			localtargetuser->Send(ServerInstance->GetRFCEvents().invite, invitemsg);
		}

		if (IS_LOCAL(user))
		{
			user->WriteNumeric(RPL_INVITING, u->nick, c->name);
			if (u->IsAway())
				user->WriteNumeric(RPL_AWAY, u->nick, u->awaymsg);
		}

		char prefix = 0;
		unsigned int minrank = 0;
		switch (announceinvites)
		{
			case Invite::ANNOUNCE_OPS:
			{
				prefix = '@';
				minrank = OP_VALUE;
				break;
			}
			case Invite::ANNOUNCE_DYNAMIC:
			{
				PrefixMode* mh = ServerInstance->Modes->FindPrefixMode('h');
				if ((mh) && (mh->name == "halfop"))
				{
					prefix = mh->GetPrefix();
					minrank = mh->GetPrefixRank();
				}
				break;
			}
			default:
			{
			}
		}

		CUList excepts;
		FOREACH_MOD(OnUserInvite, (user, u, c, timeout, minrank, excepts));

		if (announceinvites != Invite::ANNOUNCE_NONE)
		{
			excepts.insert(user);
			ClientProtocol::Messages::Privmsg privmsg(ServerInstance->FakeClient, c, InspIRCd::Format("*** %s invited %s into the channel", user->nick.c_str(), u->nick.c_str()), MSG_NOTICE);
			c->Write(ServerInstance->GetRFCEvents().privmsg, privmsg, prefix, excepts);
		}
	}
	else if (IS_LOCAL(user))
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		const Invite::List* list = invapi.GetList(IS_LOCAL(user));
		if (list)
		{
			for (Invite::List::const_iterator i = list->begin(); i != list->end(); ++i)
				user->WriteNumeric(RPL_INVITELIST, (*i)->chan->name);
		}
		user->WriteNumeric(RPL_ENDOFINVITELIST, "End of INVITE list");
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandInvite::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
