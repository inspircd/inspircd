/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "commands/cmd_invite.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandInvite(Instance);
}

/** Handle /INVITE
 */
CmdResult CommandInvite::Handle (const std::vector<std::string>& parameters, User *user)
{
	int MOD_RESULT = 0;

	if (parameters.size() == 2 || parameters.size() == 3)
	{
		User* u = ServerInstance->FindNick(parameters[0]);
		Channel* c = ServerInstance->FindChan(parameters[1]);
		time_t timeout = 0;
		if (parameters.size() == 3)
		{
			if (IS_LOCAL(user))
				timeout = ServerInstance->Time() + ServerInstance->Duration(parameters[2]);
			else
				timeout = ConvToInt(parameters[2]);
		}

		if ((!c) || (!u))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), c ? parameters[0].c_str() : parameters[1].c_str());
			return CMD_FAILURE;
		}

		if (c->HasUser(u))
	 	{
	 		user->WriteNumeric(ERR_USERONCHANNEL, "%s %s %s :is already on channel",user->nick.c_str(),u->nick.c_str(),c->name.c_str());
	 		return CMD_FAILURE;
		}

		if ((IS_LOCAL(user)) && (!c->HasUser(user)))
	 	{
			user->WriteNumeric(ERR_NOTONCHANNEL, "%s %s :You're not on that channel!",user->nick.c_str(), c->name.c_str());
	  		return CMD_FAILURE;
		}

		FOREACH_RESULT(I_OnUserPreInvite,OnUserPreInvite(user,u,c,timeout));

		if (MOD_RESULT == 1)
		{
			return CMD_FAILURE;
		}
		else if (MOD_RESULT == 0)
		{
			if (IS_LOCAL(user))
			{
				if (c->GetStatus(user) < STATUS_HOP)
				{
					user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must be a channel %soperator", user->nick.c_str(), c->name.c_str(), c->GetStatus(u) == STATUS_HOP ? "" : "half-");
					return CMD_FAILURE;
				}
			}
		}

		u->InviteTo(c->name.c_str(), timeout);
		u->WriteFrom(user,"INVITE %s :%s",u->nick.c_str(),c->name.c_str());
		user->WriteNumeric(RPL_INVITING, "%s %s %s",user->nick.c_str(),u->nick.c_str(),c->name.c_str());
		switch (ServerInstance->Config->AnnounceInvites)
		{
			case ServerConfig::INVITE_ANNOUNCE_ALL:
				c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
			break;
			case ServerConfig::INVITE_ANNOUNCE_OPS:
				c->WriteAllExceptSender(user, true, '@', "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
			break;
			case ServerConfig::INVITE_ANNOUNCE_DYNAMIC:
				if (c->IsModeSet('i'))
					c->WriteAllExceptSender(user, true, '@', "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
				else
					c->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :*** %s invited %s into the channel", c->name.c_str(), user->nick.c_str(), u->nick.c_str());
			break;
			default:
				/* Nobody */
			break;
		}
		FOREACH_MOD(I_OnUserInvite,OnUserInvite(user,u,c,timeout));
	}
	else
	{
		// pinched from ircu - invite with not enough parameters shows channels
		// youve been invited to but haven't joined yet.
		InvitedList* il = user->GetInviteList();
		for (InvitedList::iterator i = il->begin(); i != il->end(); i++)
		{
			user->WriteNumeric(RPL_INVITELIST, "%s :%s", user->nick.c_str(), i->first->name.c_str());
		}
		user->WriteNumeric(RPL_ENDOFINVITELIST, "%s :End of INVITE list",user->nick.c_str());
	}
	return CMD_SUCCESS;
}

