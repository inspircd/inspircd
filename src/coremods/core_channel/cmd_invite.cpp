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

CommandInvite::CommandInvite(Module* parent)
	: Command(parent, "INVITE", 0, 0)
{
	Penalty = 4;
	syntax = "[<nick> <channel>]";
}

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle (const std::vector<std::string>& parameters, User *user)
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
				timeout = ServerInstance->Time() + InspIRCd::Duration(parameters[2]);
			else if (parameters.size() > 3)
				timeout = ConvToInt(parameters[3]);
		}

		if ((!c) || (!u) || (u->registered != REG_ALL))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s :No such nick/channel", c ? parameters[0].c_str() : parameters[1].c_str());
			return CMD_FAILURE;
		}

		// Verify channel timestamp if the INVITE is coming from a remote server
		if (!IS_LOCAL(user))
		{
			// Remote INVITE commands must carry a channel timestamp
			if (parameters.size() < 3)
				return CMD_INVALID;

			// Drop the invite if our channel TS is lower
			time_t RemoteTS = ConvToInt(parameters[2]);
			if (c->age < RemoteTS)
				return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, "%s :You're not on that channel!", c->name.c_str());
			return CMD_FAILURE;
		}

		if (c->HasUser(u))
		{
			user->WriteNumeric(ERR_USERONCHANNEL, "%s %s :is already on channel", u->nick.c_str(), c->name.c_str());
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
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s :You must be a channel %soperator",
						c->name.c_str(), (mh && mh->name == "halfop" ? "half-" : ""));
					return CMD_FAILURE;
				}
			}
		}

		if (IS_LOCAL(u))
		{
			Invitation::Create(c, IS_LOCAL(u), timeout);
			u->WriteFrom(user,"INVITE %s :%s",u->nick.c_str(),c->name.c_str());
		}

		if (IS_LOCAL(user))
		{
			user->WriteNumeric(RPL_INVITING, "%s %s", u->nick.c_str(),c->name.c_str());
			if (u->IsAway())
				user->WriteNumeric(RPL_AWAY, "%s :%s", u->nick.c_str(), u->awaymsg.c_str());
		}

		if (ServerInstance->Config->AnnounceInvites != ServerConfig::INVITE_ANNOUNCE_NONE)
		{
			char prefix;
			switch (ServerInstance->Config->AnnounceInvites)
			{
				case ServerConfig::INVITE_ANNOUNCE_OPS:
				{
					prefix = '@';
					break;
				}
				case ServerConfig::INVITE_ANNOUNCE_DYNAMIC:
				{
					PrefixMode* mh = ServerInstance->Modes->FindPrefixMode('h');
					prefix = (mh && mh->name == "halfop" ? mh->GetPrefix() : '@');
					break;
				}
				default:
				{
					prefix = 0;
					break;
				}
			}
			c->WriteAllExceptSender(user, true, prefix, "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
		}
		FOREACH_MOD(OnUserInvite, (user,u,c,timeout));
	}
	else if (IS_LOCAL(user))
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InviteList& il = IS_LOCAL(user)->GetInviteList();
		for (InviteList::const_iterator i = il.begin(); i != il.end(); ++i)
		{
			user->WriteNumeric(RPL_INVITELIST, ":%s", (*i)->chan->name.c_str());
		}
		user->WriteNumeric(RPL_ENDOFINVITELIST, ":End of INVITE list");
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandInvite::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
