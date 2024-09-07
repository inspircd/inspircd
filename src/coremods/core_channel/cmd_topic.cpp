/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2018, 2020-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "numerichelper.h"

#include "core_channel.h"

enum
{
	// From RFC 1459.
	RPL_NOTOPICSET = 331,
	RPL_TOPIC = 332,

	// From ircu.
	RPL_TOPICTIME = 333,
};

CommandTopic::CommandTopic(Module* parent)
	: SplitCommand(parent, "TOPIC", 1, 2)
	, exemptionprov(parent)
	, secretmode(parent, "secret")
	, topiclockmode(parent, "topiclock")
{
	allow_empty_last_param = true;
	penalty = 2000;
	syntax = { "<channel> [:<topic>]" };
}

CmdResult CommandTopic::HandleLocal(LocalUser* user, const Params& parameters)
{
	auto* c = ServerInstance->Channels.Find(parameters[0]);
	if (!c)
	{
		user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
		return CmdResult::FAILURE;
	}

	if (parameters.size() == 1)
	{
		if ((c->IsModeSet(secretmode)) && (!c->HasUser(user) && !user->HasPrivPermission("channels/auspex")))
		{
			user->WriteNumeric(Numerics::NoSuchChannel(c->name));
			return CmdResult::FAILURE;
		}

		if (c->topic.length())
		{
			Topic::ShowTopic(user, c);
		}
		else
		{
			user->WriteNumeric(RPL_NOTOPICSET, c->name, "No topic is set.");
		}
		return CmdResult::SUCCESS;
	}

	std::string t = parameters[1]; // needed, in case a module wants to change it
	ModResult res;
	FIRST_MOD_RESULT(OnPreTopicChange, res, (user, c, t));

	if (res == MOD_RES_DENY)
		return CmdResult::FAILURE;
	if (res != MOD_RES_ALLOW)
	{
		if (!c->HasUser(user))
		{
			user->WriteNumeric(ERR_NOTONCHANNEL, c->name, "You're not on that channel!");
			return CmdResult::FAILURE;
		}
		if (c->IsModeSet(topiclockmode))
		{
			ModResult modres = exemptionprov.Check(user, c, "topiclock");
			if (!modres.check(c->GetPrefixValue(user) >= HALFOP_VALUE))
			{
				user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(c, HALFOP_VALUE, "change the topic"));
				return CmdResult::FAILURE;
			}
		}
	}

	// Make sure the topic is not longer than the limit in the config
	if (t.length() > ServerInstance->Config->Limits.MaxTopic)
		t.erase(ServerInstance->Config->Limits.MaxTopic);

	// Only change if the new topic is different than the current one
	if (c->topic != t)
		c->SetTopic(user, t, ServerInstance->Time());
	return CmdResult::SUCCESS;
}

void Topic::ShowTopic(LocalUser* user, Channel* chan)
{
	user->WriteNumeric(RPL_TOPIC, chan->name, chan->topic);
	user->WriteNumeric(RPL_TOPICTIME, chan->name, chan->setby, chan->topicset);
}
