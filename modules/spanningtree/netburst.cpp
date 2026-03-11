/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "listmode.h"
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "main.h"
#include "commands.h"

/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	ServerInstance->SNO.WriteToSnoMask('l', "Bursting to \002{}\002 (Authentication: {}{}).",
		s->GetName(),
		capab->auth_fingerprint ? "TLS certificate fingerprint and " : "",
		capab->auth_challenge ? "challenge-response" : "plaintext password");
	this->CleanNegotiationInfo();

	MessageBuilder("BURST").Push(ServerInstance->Time()).Unicast(this);

	// Introduce all servers behind us
	this->SendServers(Utils->TreeRoot, s);

	// Introduce all users
	this->SendUsers(s);

	// Sync all channels
	for (const auto& [_, chan] : ServerInstance->Channels.GetChans())
		SyncChannel(chan, s);

	// Send all xlines
	this->SendXLines();
	Utils->Creator->synceventprov.Call(&ServerProtocol::SyncEventListener::OnSyncNetwork, *s);
	MessageBuilder("ENDBURST").Unicast(this);
	ServerInstance->SNO.WriteToSnoMask('l', "Finished bursting to \002"+ s->GetName()+"\002.");
}

void TreeSocket::SendServerInfo(TreeServer* from)
{
	CommandSInfo::Builder(from, "customversion", from->customversion).Unicast(this);
	CommandSInfo::Builder(from, "rawbranch", from->rawbranch).Unicast(this);
	CommandSInfo::Builder(from, "rawversion", from->rawversion).Unicast(this);
}

/** Recursively send the server tree.
 * This is used during network burst to inform the other server
 * (and any of ITS servers too) of what servers we know about.
 * If at any point any of these servers already exist on the other
 * end, our connection may be terminated.
 */
void TreeSocket::SendServers(TreeServer* Current, TreeServer* s)
{
	SendServerInfo(Current);

	for (auto* recursive_server : Current->GetChildren())
	{
		if (recursive_server != s)
		{
			CommandServer::Builder(recursive_server).Unicast(this);
			/* down to next level */
			this->SendServers(recursive_server, s);
		}
	}
}

// Send one or more FJOINs for a channel of users.
void TreeSocket::SendFJoins(Channel* chan)
{
	CommandFJoin::Builder fjoin(chan);
	for (const auto& [_, memb] : chan->GetUsers())
		fjoin.add(memb);

	fjoin.Unicast(this);
}

/** Send all XLines we know about */
void TreeSocket::SendXLines()
{
	for (const auto& xltype : ServerInstance->XLines->GetAllTypes())
	{
		/* Expired lines are removed in XLineManager::GetAll() */
		XLineLookup* lookup = ServerInstance->XLines->GetAll(xltype);

		/* lookup cannot be NULL in this case but a check won't hurt */
		if (lookup)
		{
			for (const auto& [_, xline] : *lookup)
			{
				/* Is it burstable? this is better than an explicit check for type 'K'.
				 * We break the loop as NONE of the items in this group are worth iterating.
				 */
				if (xline->IsBurstable())
					CommandAddLine::Builder(xline).Unicast(this);
			}
		}
	}
}

void TreeSocket::SendListModes(Channel* chan)
{
	for (auto* mode : ServerInstance->Modes.GetListModes())
	{
		ListModeBase::ModeList* list = mode->GetList(chan);
		if (!list || list->empty())
			continue;

		MessageBuilder msg("LMODE");
		msg.Push(chan->name, chan->age, mode->GetModeChar());

		for (const auto& entry : *list)
			msg.Push(entry.mask, entry.setter, entry.time);

		msg.Unicast(this);
	}
}

/** Send channel users, topic, modes and global metadata */
void TreeSocket::SyncChannel(Channel* chan, TreeServer* s)
{
	SendFJoins(chan);

	// If the topic was ever set, send it, even if it's empty now
	// because a new empty topic should override an old non-empty topic
	if (chan->topicset != 0)
		CommandFTopic::Builder(chan).Unicast(this);

	SpanningTreeUtilities::SendListLimits(chan, this);
	SendListModes(chan);

	for (const auto& [item, value] : chan->GetExtList())
	{
		const std::string valuestr = item->ToNetwork(chan, value);
		if (!valuestr.empty())
		{
			CommandMetadata::Builder(chan, item->service_name, valuestr).Unicast(this);
			item->OnSync(chan, value, s);
		}
	}

	for (const auto& [_, memb] : chan->GetUsers())
	{
		for (const auto& [item, value] : memb->GetExtList())
		{
			const std::string valuestr = item->ToNetwork(memb, value);
			if (!valuestr.empty())
			{
				CommandMetadata::Builder(memb, item->service_name, valuestr).Unicast(this);
				item->OnSync(memb, value, s);
			}
		}
	}

	Utils->Creator->synceventprov.Call(&ServerProtocol::SyncEventListener::OnSyncChannel, chan, *s);
}


/** Send all users and their state, including oper and away status and global metadata */
void TreeSocket::SendUsers(TreeServer* s)
{
	for (const auto& [_, user] : ServerInstance->Users.GetUsers())
	{
		if (!user->IsFullyConnected())
			continue;

		CommandUID::Builder(user).Unicast(this);

		if (user->IsOper())
			CommandOpertype::Builder(user, user->oper).Unicast(this);

		if (user->IsAway())
			CommandAway::Builder(user).Unicast(this);

		if (user->uniqueusername) // TODO: convert this to BooleanExtItem.
			CommandMetadata::Builder(user, "uniqueusername", "1").Unicast(this);

		for (const auto& [item, obj] : user->GetExtList())
		{
			const std::string value = item->ToNetwork(user, obj);
			if (!value.empty())
			{
				CommandMetadata::Builder(user, item->service_name, value).Unicast(this);
				item->OnSync(user, obj, s);
			}
		}

		Utils->Creator->synceventprov.Call(&ServerProtocol::SyncEventListener::OnSyncUser, user, *s);
	}
}
