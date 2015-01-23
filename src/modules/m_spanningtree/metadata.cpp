/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

CmdResult CommandMetadata::Handle(User* srcuser, std::vector<std::string>& params)
{
	if (params[0] == "*")
	{
		std::string value = params.size() < 3 ? "" : params[2];
		FOREACH_MOD(OnDecodeMetaData, (NULL,params[1],value));
		return CMD_SUCCESS;
	}

	if (params[0][0] == '#')
	{
		// Channel METADATA has an additional parameter: the channel TS
		// :22D METADATA #channel 12345 extname :extdata
		if (params.size() < 3)
			throw ProtocolException("Insufficient parameters for channel METADATA");

		Channel* c = ServerInstance->FindChan(params[0]);
		if (!c)
			return CMD_FAILURE;

		time_t ChanTS = ServerCommand::ExtractTS(params[1]);
		if (c->age < ChanTS)
			// Their TS is newer than ours, discard this command and do not propagate
			return CMD_FAILURE;

		std::string value = params.size() < 4 ? "" : params[3];

		ExtensionItem* item = ServerInstance->Extensions.GetItem(params[2]);
		if ((item) && (item->type == ExtensionItem::EXT_CHANNEL))
			item->unserialize(FORMAT_NETWORK, c, value);
		FOREACH_MOD(OnDecodeMetaData, (c,params[2],value));
	}
	else
	{
		User* u = ServerInstance->FindUUID(params[0]);
		if ((u) && (!IS_SERVER(u)))
		{
			ExtensionItem* item = ServerInstance->Extensions.GetItem(params[1]);
			std::string value = params.size() < 3 ? "" : params[2];

			if ((item) && (item->type == ExtensionItem::EXT_USER))
				item->unserialize(FORMAT_NETWORK, u, value);
			FOREACH_MOD(OnDecodeMetaData, (u,params[1],value));
		}
	}

	return CMD_SUCCESS;
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

CommandMetadata::Builder::Builder(const std::string& key, const std::string& val)
	: CmdBuilder("METADATA")
{
	push("*");
	push(key);
	push_last(val);
}
