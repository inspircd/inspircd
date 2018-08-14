/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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

class RFCSerializer : public ClientProtocol::Serializer
{
	/** Maximum size of the message tags portion of the message, including the `@` and the trailing space characters.
	 */
	static const std::string::size_type MAX_MESSAGE_TAG_LENGTH = 512;

	static void SerializeTags(const ClientProtocol::TagMap& tags, const ClientProtocol::TagSelection& tagwl, std::string& line);

 public:
	RFCSerializer(Module* mod)
		: ClientProtocol::Serializer(mod, "rfc")
	{
	}

 	bool Parse(LocalUser* user, const std::string& line, ClientProtocol::ParseOutput& parseoutput) CXX11_OVERRIDE;
	ClientProtocol::SerializedMessage Serialize(const ClientProtocol::Message& msg, const ClientProtocol::TagSelection& tagwl) const CXX11_OVERRIDE;
};

bool RFCSerializer::Parse(LocalUser* user, const std::string& line, ClientProtocol::ParseOutput& parseoutput)
{
	size_t start = line.find_first_not_of(" ");
	if (start == std::string::npos)
	{
		// Discourage the user from flooding the server.
		user->CommandFloodPenalty += 2000;
		return false;
	}

	ServerInstance->Logs->Log("USERINPUT", LOG_RAWIO, "C[%s] I %s", user->uuid.c_str(), line.c_str());

	irc::tokenstream tokens(line, start);
	std::string token;

	// This will always exist because of the check at the start of the function.
	tokens.GetMiddle(token);
	if (token[0] == '@')
	{
		// Line begins with message tags, parse them.
		std::string tagval;
		irc::sepstream ss(token.substr(1), ';');
		while (ss.GetToken(token))
		{
			// Two or more tags with the same key must not be sent, but if a client violates that we accept
			// the first occurence of duplicate tags and ignore all later occurences.
			//
			// Another option is to reject the message entirely but there is no standard way of doing that.
			const std::string::size_type p = token.find('=');
			if (p != std::string::npos)
			{
				// Tag has a value
				tagval.assign(token, p+1, std::string::npos);
				token.erase(p);
			}
			else
				tagval.clear();

			HandleTag(user, token, tagval, parseoutput.tags);
		}


		// Try to read the prefix or command name.
		if (!tokens.GetMiddle(token))
		{
			// Discourage the user from flooding the server.
			user->CommandFloodPenalty += 2000;
			return false;
		}
	}

	if (token[0] == ':')
	{
		// If this exists then the client sent a prefix as part of their
		// message. Section 2.3 of RFC 1459 technically says we should only
		// allow the nick of the client here but in practise everyone just
		// ignores it so we will copy them.

		// Try to read the command name.
		if (!tokens.GetMiddle(token))
		{
			// Discourage the user from flooding the server.
			user->CommandFloodPenalty += 2000;
			return false;
		}
	}

	parseoutput.cmd.assign(token);

	// Build the parameter map. We intentionally do not respect the RFC 1459
	// thirteen parameter limit here.
	while (tokens.GetTrailing(token))
		parseoutput.params.push_back(token);

	return true;
}

void RFCSerializer::SerializeTags(const ClientProtocol::TagMap& tags, const ClientProtocol::TagSelection& tagwl, std::string& line)
{
	char prefix = '@'; // First tag name is prefixed with a '@'
	for (ClientProtocol::TagMap::const_iterator i = tags.begin(); i != tags.end(); ++i)
	{
		if (!tagwl.IsSelected(tags, i))
			continue;

		const std::string::size_type prevsize = line.size();
		line.push_back(prefix);
		prefix = ';'; // Remaining tags are prefixed with ';'
		line.append(i->first);
		const std::string& val = i->second.value;
		if (!val.empty())
		{
			line.push_back('=');
			line.append(val);
		}

		// The tags part of the message mustn't grow longer than what is allowed by the spec. If it does,
		// remove last tag and stop adding more tags.
		//
		// One is subtracted from the limit before comparing because there must be a ' ' char after the last tag
		// which also counts towards the limit.
		if (line.size() > MAX_MESSAGE_TAG_LENGTH-1)
		{
			line.erase(prevsize);
			break;
		}
	}

	if (!line.empty())
		line.push_back(' ');
}

ClientProtocol::SerializedMessage RFCSerializer::Serialize(const ClientProtocol::Message& msg, const ClientProtocol::TagSelection& tagwl) const
{
	std::string line;
	SerializeTags(msg.GetTags(), tagwl, line);

	// Save position for length calculation later
	const std::string::size_type rfcmsg_begin = line.size();

	if (msg.GetSource())
	{
		line.push_back(':');
		line.append(*msg.GetSource());
		line.push_back(' ');
	}
	line.append(msg.GetCommand());

	const ClientProtocol::Message::ParamList& params = msg.GetParams();
	if (!params.empty())
	{
		for (ClientProtocol::Message::ParamList::const_iterator i = params.begin(); i != params.end()-1; ++i)
		{
			const std::string& param = *i;
			line.push_back(' ');
			line.append(param);
		}

		line.append(" :", 2).append(params.back());
	}

	// Truncate if too long
	std::string::size_type maxline = ServerInstance->Config->Limits.MaxLine - 2;
	if (line.length() - rfcmsg_begin > maxline)
		line.erase(rfcmsg_begin + maxline);

	line.append("\r\n", 2);
	return line;
}

class ModuleCoreRFCSerializer : public Module
{
	RFCSerializer rfcserializer;

 public:
	ModuleCoreRFCSerializer()
		: rfcserializer(this)
	{
	}

	void OnCleanup(ExtensionItem::ExtensibleType type, Extensible* item) CXX11_OVERRIDE
	{
		if (type != ExtensionItem::EXT_USER)
			return;

		LocalUser* const user = IS_LOCAL(static_cast<User*>(item));
		if ((user) && (user->serializer == &rfcserializer))
			ServerInstance->Users.QuitUser(user, "Protocol serializer module unloading");
	}

	void OnUserInit(LocalUser* user) CXX11_OVERRIDE
	{
		if (!user->serializer)
			user->serializer = &rfcserializer;
	}

	Version GetVersion()
	{
		return Version("RFC client protocol serializer and unserializer", VF_CORE|VF_VENDOR);
	}
};

MODULE_INIT(ModuleCoreRFCSerializer)
