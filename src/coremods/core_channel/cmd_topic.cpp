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

CommandTopic::CommandTopic(Module* parent)
	: SplitCommand(parent, "TOPIC", 1, 2)
	, exemptionprov(parent)
	, secretmode(parent, "secret")
	, topiclockmode(parent, "topiclock")
{
	syntax = "<channel> [<topic>]";
	Penalty = 2;
}

CmdResult CommandTopic::HandleLocal(const std::vector<std::string>& parameters, LocalUser* user)
{
	Channel* c = ServerInstance->FindChan(parameters[0]);
	if (!c)
	{
		user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
		return CMD_FAILURE;
	}

	if (parameters.size() == 1)
	{
		if ((c->IsModeSet(secretmode)) && (!c->HasUser(user)))
		{
			user->WriteNumeric(Numerics::NoSuchNick(c->name));
			return CMD_FAILURE;
		}

		if (c->topic.length())
		{
			Topic::ShowTopic(user, c);
		}
		else
		{
			user->WriteNumeric(RPL_NOTOPICSET, c->name, "No topic is set.");
		}
		return CMD_SUCCESS;
	}

	std::string t = parameters[1]; // needed, in case a module wants to change it
	ModResult res;
	FIRST_MOD_RESULT(OnPreTopicChange, res, (user,c,t));

	if (res == MOD_RES_DENY)
		return CMD_FAILURE;
	if (res != MOD_RES_ALLOW)
	{
		if (!c->HasUser(user))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, c->name, "You're not on that channel!");
			return CMD_FAILURE;
		}
		if (c->IsModeSet(topiclockmode))
		{
			ModResult MOD_RESULT;
			FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, MOD_RESULT, (user, c, "topiclock"));
			if (!MOD_RESULT.check(c->GetPrefixValue(user) >= HALFOP_VALUE))
			{
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, c->name, "You do not have access to change the topic on this channel");
				return CMD_FAILURE;
			}
		}
	}

	// Make sure the topic is not longer than the limit in the config
	if (t.length() > ServerInstance->Config->Limits.MaxTopic)
		t.erase(ServerInstance->Config->Limits.MaxTopic);

	// Only change if the new topic is different than the current one
	if (c->topic != t)
		c->SetTopic(user, t, ServerInstance->Time());
	return CMD_SUCCESS;
}

void Topic::ShowTopic(LocalUser* user, Channel* chan)
{
	user->WriteNumeric(RPL_TOPIC, chan->name, chan->topic);
	user->WriteNumeric(RPL_TOPICTIME, chan->name, chan->setby, (unsigned long)chan->topicset);
}
