/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

#include <sstream>

#include "main.h"
#include "msgbuilder.h"

void MessageBuilder::Broadcast(const TreeServer* omit)
{
	for (const auto* server : Utils->TreeRoot->GetChildren())
	{
		if (!omit || omit != server)
			Unicast(server->GetSocket());
	}
}

MessageBuilder& MessageBuilder::PushParams(const CommandBase::Params& newparams)
{
	for (const auto& param : newparams)
		Push(param);
	PushTags(newparams.GetTags());
	return *this;
}

MessageBuilder& MessageBuilder::PushTags(ClientProtocol::TagMap newtags)
{
	auto& oldtags = this->parameters.GetTags();
	newtags.insert(oldtags.begin(), oldtags.end());
	std::swap(oldtags, newtags);
	return *this;
}

void MessageBuilder::Finalize()
{
	if (!Utils || this->finalized)
		return; // Should never happen

	Utils->Creator->messageeventprov.Call(&ServerProtocol::MessageEventListener::OnServerMessage,
		this->source, this->command, this->parameters);

	finalized = true;
}

std::string MessageBuilder::ToRFC1459() const
{
	std::stringstream buffer;

	const auto& tags = this->parameters.GetTags();
	if (!tags.empty())
	{
		auto separator = '@';
		for (const auto &[tname, tinfo] : tags)
		{
			buffer << separator << tname;
			if (!tinfo.value.empty())
				buffer << '=' << tinfo.value;
			separator = ';';
		}
		if (separator != '@')
			buffer << ' ';
	}

	if (this->source)
		buffer << ':' << source->uuid << ' ';

	buffer << command;
	if (!this->parameters.empty())
	{
		buffer << ' ';
		for (auto it = this->parameters.begin(); it != this->parameters.end() - 1; ++it)
			buffer << *it << ' ';

		const auto &last = this->parameters.back();
		if (last.empty() || last[0] == ':' || last.find(' ') != std::string::npos)
			buffer << ':';
		buffer << last;
	}

	return buffer.str();
}

void MessageBuilder::Unicast(TreeServer* server)
{
	if (server->GetSocket())
		Unicast(server->GetSocket());
}

void MessageBuilder::Unicast(TreeSocket* socket)
{
	this->Finalize();
	socket->SendMessage(*this);
}

void MessageBuilder::Unicast(const User* user)
{
	if (user->server)
		Unicast(static_cast<TreeServer*>(user->server));
}
