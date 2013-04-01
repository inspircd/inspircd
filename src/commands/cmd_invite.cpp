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

/** Handle /INVITE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandInvite : public Command
{
 public:
	/** Constructor for invite.
	 */
	CommandInvite ( Module* parent) : Command(parent,"INVITE", 0, 0) { Penalty = 4; syntax = "[<nick> <channel>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle (const std::vector<std::string>& parameters, User *user)
{
	ModResult MOD_RESULT;

	if (parameters.size() == 2 || parameters.size() == 3)
	{
		User* u;
		if (IS_LOCAL(user))
			u = ServerInstance->FindNickOnly(parameters[0]);
		else
			u = ServerInstance->FindNick(parameters[0]);

		Channel* c = ServerInstance->FindChan(parameters[1]);
		time_t timeout = 0;
		if (parameters.size() == 3)
		{
			if (IS_LOCAL(user))
				timeout = ServerInstance->Time() + ServerInstance->Duration(parameters[2]);
			else
				timeout = ConvToInt(parameters[2]);
		}

		if ((!c) || (!u) || (u->registered != REG_ALL))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), c ? parameters[0].c_str() : parameters[1].c_str());
			return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You're not on that channel!",user->nick.c_str(), c->name.c_str());
			return CMD_FAILURE;
		}

		if (c->HasUser(u))
		{
			user->WriteNumeric(ERR_USERONCHANNEL, "%s %s %s :is already on channel",user->nick.c_str(),u->nick.c_str(),c->name.c_str());
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
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator",
						user->nick.c_str(), c->name.c_str(), (mh && mh->name == "halfop" ? "half-" : ""));
					return CMD_FAILURE;
				}
			}
		}

		if (IS_LOCAL(u))
			IS_LOCAL(u)->InviteTo(c->name.c_str(), timeout);
		u->WriteFrom(user,"INVITE %s :%s",u->nick.c_str(),c->name.c_str());
		user->WriteNumeric(RPL_INVITING, "%s %s %s",user->nick.c_str(),u->nick.c_str(),c->name.c_str());
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
					ModeHandler* mh = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
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
		FOREACH_MOD(I_OnUserInvite,OnUserInvite(user,u,c,timeout));
	}
	else if (IS_LOCAL(user))
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InviteList& il = IS_LOCAL(user)->GetInviteList();
		for (InviteList::const_iterator i = il.begin(); i != il.end(); ++i)
		{
			user->WriteNumeric(RPL_INVITELIST, "%s :%s",user->nick.c_str(), (*i)->chan->name.c_str());
		}
		user->WriteNumeric(RPL_ENDOFINVITELIST, "%s :End of INVITE list",user->nick.c_str());
	}
	return CMD_SUCCESS;
}


COMMAND_INIT(CommandInvite)
