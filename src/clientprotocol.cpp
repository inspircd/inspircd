/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Attila Molnar <attilamolnar@hush.com>
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

ClientProtocol::Serializer::Serializer(Module* mod, const std::string& Name)
	: DataProvider(mod, "serializer/" + Name)
	, evprov(mod)
{
}

bool ClientProtocol::Serializer::HandleTag(LocalUser* user, const std::string& tagname, std::string& tagvalue, TagMap& tags) const
{
	// Catch and block empty tags
	if (tagname.empty())
		return false;

	for (auto* subscriber : evprov.GetSubscribers())
	{
		MessageTagProvider* const tagprov = static_cast<MessageTagProvider*>(subscriber);
		const ModResult res = tagprov->OnProcessTag(user, tagname, tagvalue);
		if (res == MOD_RES_ALLOW)
			return tags.emplace(tagname, MessageTagData(tagprov, tagvalue)).second;
		else if (res == MOD_RES_DENY)
			break;
	}

	// No module handles the tag but that's not an error
	return true;
}

ClientProtocol::TagSelection ClientProtocol::Serializer::MakeTagWhitelist(LocalUser* user, const TagMap& tagmap)
{
	TagSelection tagwl;
	for (TagMap::const_iterator i = tagmap.begin(); i != tagmap.end(); ++i)
	{
		const MessageTagData& tagdata = i->second;
		if (tagdata.tagprov->ShouldSendTag(user, tagdata))
			tagwl.Select(tagmap, i);
	}
	return tagwl;
}

const ClientProtocol::SerializedMessage& ClientProtocol::Serializer::SerializeForUser(LocalUser* user, Message& msg)
{
	if (!msg.msginit_done)
	{
		msg.msginit_done = true;
		evprov.Call(&MessageTagProvider::OnPopulateTags, msg);
	}
	return msg.GetSerialized(Message::SerializedInfo(this, MakeTagWhitelist(user, msg.GetTags())));
}

std::string ClientProtocol::Message::EscapeTag(const std::string& value)
{
	std::string ret;
	ret.reserve(value.size());
	for (const auto chr : value)
	{
		switch (chr)
		{
			case ' ':
				ret.append("\\s");
				break;
			case ';':
				ret.append("\\:");
				break;
			case '\\':
				ret.append("\\\\");
				break;
			case '\n':
				ret.append("\\n");
				break;
			case '\r':
				ret.append("\\r");
				break;
			default:
				ret.push_back(chr);
				break;
		}
	}
	return ret;
}

std::string ClientProtocol::Message::UnescapeTag(const std::string& value)
{
	std::string ret;
	ret.reserve(value.size());
	for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
	{
		char chr = *it;
		if (chr != '\\')
		{
			ret.push_back(chr);
			continue;
		}

		it++;
		if (it == value.end())
			break;

		chr = *it;
		switch (chr)
		{
			case 's':
				ret.push_back(' ');
				break;
			case ':':
				ret.push_back(';');
				break;
			case '\\':
				ret.push_back('\\');
				break;
			case 'n':
				ret.push_back('\n');
				break;
			case 'r':
				ret.push_back('\r');
				break;
			default:
				ret.push_back(chr);
				break;
		}
	}
	return ret;
}

const ClientProtocol::SerializedMessage& ClientProtocol::Message::GetSerialized(const SerializedInfo& serializeinfo) const
{
	// First check if the serialized line they're asking for is in the cache
	for (const auto& [info, msg] : serlist)
	{
		if (info == serializeinfo)
			return msg;
	}

	// Not cached, generate it and put it in the cache for later use
	serlist.push_back(std::make_pair(serializeinfo, serializeinfo.serializer->Serialize(*this, serializeinfo.tagwl)));
	return serlist.back().second;
}

void ClientProtocol::Event::GetMessagesForUser(LocalUser* user, MessageList& messagelist)
{
	if (!eventinit_done)
	{
		eventinit_done = true;
		event->Call(&EventHook::OnEventInit, *this);
	}

	// Most of the time there's only a single message but in rare cases there are more
	if (initialmsg)
		messagelist.assign(1, initialmsg);
	else
		messagelist = *initialmsglist;

	// Let modules modify the message list
	ModResult res = event->FirstResult(&EventHook::OnPreEventSend, user, *this, messagelist);
	if (res == MOD_RES_DENY)
		messagelist.clear();
}
