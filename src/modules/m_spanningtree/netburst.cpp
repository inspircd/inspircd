/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
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

/**
 * Creates FMODE messages, used only when syncing channels
 */
class FModeBuilder final
	: public CmdBuilder
{
	static constexpr size_t maxline = 480;
	std::string params;
	unsigned int modes = 0;
	std::string::size_type startpos;

public:
	FModeBuilder(Channel* chan)
		: CmdBuilder("FMODE")
	{
		push(chan->name).push_int(chan->age).push_raw(" +");
		startpos = str().size();
	}

	/** Add a mode to the message
	 */
	void push_mode(char modeletter, const std::string& mask)
	{
		push_raw(modeletter);
		params.push_back(' ');
		params.append(mask);
		modes++;
	}

	/** Remove all modes from the message
	 */
	void clear()
	{
		content.erase(startpos);
		params.clear();
		modes = 0;
	}

	/** Prepare the message for sending, next mode can only be added after clear()
	 */
	const std::string& finalize()
	{
		return push_raw(params);
	}

	/** Returns true if this message is empty (has no modes)
	 */
	bool empty() const
	{
		return (modes == 0);
	}
};

struct TreeSocket::BurstState final
{
	SpanningTreeProtocolInterface::Server server;
	BurstState(TreeSocket* sock)
		: server(sock)
	{
	}
};

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
	this->WriteLine(CmdBuilder("BURST").push_int(ServerInstance->Time()));
	// Introduce all servers behind us
	this->SendServers(Utils->TreeRoot, s);

	BurstState bs(this);
	// Introduce all users
	this->SendUsers(bs);

	// Sync all channels
	for (const auto& [_, chan] : ServerInstance->Channels.GetChans())
		SyncChannel(chan, bs);

	// Send all xlines
	this->SendXLines();
	Utils->Creator->synceventprov.Call(&ServerProtocol::SyncEventListener::OnSyncNetwork, bs.server);
	this->WriteLine(CmdBuilder("ENDBURST"));
	ServerInstance->SNO.WriteToSnoMask('l', "Finished bursting to \002"+ s->GetName()+"\002.");
}

void TreeSocket::SendServerInfo(TreeServer* from)
{
	this->WriteLine(CommandSInfo::Builder(from, "customversion", from->customversion));
	this->WriteLine(CommandSInfo::Builder(from, "rawbranch", from->rawbranch));
	this->WriteLine(CommandSInfo::Builder(from, "rawversion", from->rawversion));

	if (proto_version < PROTO_INSPIRCD_4)
	{
		this->WriteLine(CommandSInfo::Builder(from, "version", INSP_FORMAT("{}. {} :{}", from->rawbranch,
			from->GetPublicName(), from->customversion)));

		this->WriteLine(CommandSInfo::Builder(from, "fullversion", INSP_FORMAT("{}. {} :[{}] {}", from->rawversion,
			from->GetName(), from->GetId(), from->customversion)));
	}
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
			this->WriteLine(CommandServer::Builder(recursive_server));
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

	this->WriteLine(fjoin.finalize());
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
					this->WriteLine(CommandAddLine::Builder(xline));
			}
		}
	}
}

void TreeSocket::SendListModes(Channel* chan)
{
	if (proto_version < PROTO_INSPIRCD_4)
	{
		SendLegacyListModes(chan);
		return;
	}

	for (auto* mode : ServerInstance->Modes.GetListModes())
	{
		ListModeBase::ModeList* list = mode->GetList(chan);
		if (!list || list->empty())
			continue;

		CmdBuilder lmode("LMODE");
		lmode.push(chan->name).push_int(chan->age).push(mode->GetModeChar());

		for (const auto& entry : *list)
			lmode.push(entry.mask).push(entry.setter).push_int(entry.time);

		this->WriteLine(lmode.str());
	}
}

void TreeSocket::SendLegacyListModes(Channel* chan)
{
	FModeBuilder fmode(chan);
	for (auto* mode : ServerInstance->Modes.GetListModes())
	{
		ListModeBase::ModeList* list = mode->GetList(chan);
		if (!list)
			continue;

		for (const auto& entry : *list)
			fmode.push_mode(mode->GetModeChar(), entry.mask);
	}

	if (!fmode.empty())
		this->WriteLine(fmode.finalize());
}

/** Send channel users, topic, modes and global metadata */
void TreeSocket::SyncChannel(Channel* chan, BurstState& bs)
{
	SendFJoins(chan);

	// If the topic was ever set, send it, even if it's empty now
	// because a new empty topic should override an old non-empty topic
	if (chan->topicset != 0)
		this->WriteLine(CommandFTopic::Builder(chan));

	SpanningTreeUtilities::SendListLimits(chan, this);
	SendListModes(chan);

	for (const auto& [item, value] : chan->GetExtList())
	{
		const std::string valuestr = item->ToNetwork(chan, value);
		if (!valuestr.empty())
			this->WriteLine(CommandMetadata::Builder(chan, item->name, valuestr));
	}

	for (const auto& [_, memb] : chan->GetUsers())
	{
		for (const auto& [item, value] : memb->GetExtList())
		{
			const std::string valuestr = item->ToNetwork(memb, value);
			if (!valuestr.empty())
				this->WriteLine(CommandMetadata::Builder(memb, item->name, valuestr));
		}
	}

	Utils->Creator->synceventprov.Call(&ServerProtocol::SyncEventListener::OnSyncChannel, chan, bs.server);
}

void TreeSocket::SyncChannel(Channel* chan)
{
	BurstState bs(this);
	SyncChannel(chan, bs);
}

/** Send all users and their state, including oper and away status and global metadata */
void TreeSocket::SendUsers(BurstState& bs)
{
	for (const auto& [_, user] : ServerInstance->Users.GetUsers())
	{
		if (!user->IsFullyConnected())
			continue;

		this->WriteLine(CommandUID::Builder(user, this->proto_version != PROTO_INSPIRCD_3));

		if (user->IsOper())
			this->WriteLine(CommandOpertype::Builder(user, user->oper));

		if (user->IsAway())
			this->WriteLine(CommandAway::Builder(user));

		if (user->uniqueusername) // TODO: convert this to BooleanExtItem.
			this->WriteLine(CommandMetadata::Builder(user, "uniqueusername", "1"));

		for (const auto& [item, obj] : user->GetExtList())
		{
			const std::string value = item->ToNetwork(user, obj);
			if (!value.empty())
				this->WriteLine(CommandMetadata::Builder(user, item->name, value));
		}

		Utils->Creator->synceventprov.Call(&ServerProtocol::SyncEventListener::OnSyncUser, user, bs.server);
	}
}
