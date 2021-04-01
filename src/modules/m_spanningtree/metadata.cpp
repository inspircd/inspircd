/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands.h"

CmdResult CommandMetadata::Handle(User* srcuser, Params& params)
{
	if (params[0] == "*")
	{
		std::string value = params.size() < 3 ? "" : params[2];
		FOREACH_MOD(OnDecodeMetaData, (NULL,params[1],value));
		return CmdResult::SUCCESS;
	}

	if (params[0] == "@")
	{
		// Membership METADATA has four additional parameters:
		// 1. The uuid of the membership's user.
		// 2. The name of the membership's channel.
		// 3. The channel creation timestamp.
		// 4. The membership identifier.
		//
		// :22D METADATA @ uuid #channel 12345 12345 extname :extdata
		if (params.size() < 6)
			throw ProtocolException("Insufficient parameters for channel METADATA");

		User* u = ServerInstance->Users.FindUUID(params[1]);
		if (!u)
			return CmdResult::FAILURE; // User does not exist.

		Channel* c = ServerInstance->FindChan(params[2]);
		if (!c)
			return CmdResult::FAILURE; // Channel does not exist.

		time_t chants = ServerCommand::ExtractTS(params[3]);
		if (c->age < chants)
			return CmdResult::FAILURE; // Their channel is newer than ours (probably recreated).

		Membership* m = c->GetUser(u);
		if (!m || m->id != Membership::IdFromString(params[4]))
			return CmdResult::FAILURE; // User is not in the channel.

		ExtensionItem* item = ServerInstance->Extensions.GetItem(params[5]);
		const std::string value = params.size() < 7 ? "" : params[6];
		if (item && item->type == ExtensionItem::EXT_MEMBERSHIP)
			item->FromNetwork(m, value);
		FOREACH_MOD(OnDecodeMetaData, (m, params[5], value));
	}

	if (params[0][0] == '#')
	{
		// Channel METADATA has an additional parameter: the channel TS
		// :22D METADATA #channel 12345 extname :extdata
		if (params.size() < 3)
			throw ProtocolException("Insufficient parameters for channel METADATA");

		Channel* c = ServerInstance->FindChan(params[0]);
		if (!c)
			return CmdResult::FAILURE;

		time_t ChanTS = ServerCommand::ExtractTS(params[1]);
		if (c->age < ChanTS)
			// Their TS is newer than ours, discard this command and do not propagate
			return CmdResult::FAILURE;

		std::string value = params.size() < 4 ? "" : params[3];

		ExtensionItem* item = ServerInstance->Extensions.GetItem(params[2]);
		if ((item) && (item->type == ExtensionItem::EXT_CHANNEL))
			item->FromNetwork(c, value);
		FOREACH_MOD(OnDecodeMetaData, (c,params[2],value));
	}
	else
	{
		User* u = ServerInstance->Users.FindUUID(params[0]);
		if (u)
		{
			ExtensionItem* item = ServerInstance->Extensions.GetItem(params[1]);
			std::string value = params.size() < 3 ? "" : params[2];

			if ((item) && (item->type == ExtensionItem::EXT_USER))
				item->FromNetwork(u, value);
			FOREACH_MOD(OnDecodeMetaData, (u,params[1],value));
		}
	}

	return CmdResult::SUCCESS;
}

CommandMetadata::Builder::Builder(User* user, const std::string& key, const std::string& val)
	: CmdBuilder("METADATA")
{
	push(user->uuid);
	push(key);
	push_last(val);
}

CommandMetadata::Builder::Builder(Channel* chan, const std::string& key, const std::string& val)
	: CmdBuilder("METADATA")
{
	push(chan->name);
	push_int(chan->age);
	push(key);
	push_last(val);
}

CommandMetadata::Builder::Builder(Membership* memb, const std::string& key, const std::string& val)
	: CmdBuilder("METADATA")
{
	push_raw("@");
	push(memb->user->uuid);
	push_raw(memb->chan->name);
	push_int(memb->chan->age);
	push_int(memb->id);
	push(key);
	push_last(val);
}

CommandMetadata::Builder::Builder(const std::string& key, const std::string& val)
	: CmdBuilder("METADATA")
{
	push("*");
	push(key);
	push_last(val);
}
