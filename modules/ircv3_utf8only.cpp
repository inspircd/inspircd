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

/// $CompilerFlags: require_environment("SYSTEM_UTFCPP" "1") -DUSE_SYSTEM_UTFCPP


#ifdef USE_SYSTEM_UTFCPP
# include <utf8cpp/utf8/unchecked.h>
#else
# include <utfcpp/unchecked.h>
#endif

#include "inspircd.h"
#include "modules/isupport.h"
#include "stringutils.h"

enum
{
	// From ircu.
	ERR_INPUTTOOLONG = 417
};

class UTF8Serializer final
	: public ClientProtocol::Serializer
{
private:
	// The maximum size of client-originated message tags in an incoming message including the `@`.
	static constexpr std::string::size_type MAX_CLIENT_MESSAGE_TAG_LENGTH = 4095;

	// The maximum size of server-originated message tags in an outgoing message including the `@`.
	static constexpr std::string::size_type MAX_SERVER_MESSAGE_TAG_LENGTH = 4095;

	static void SerializeTags(const ClientProtocol::TagMap& tags, const ClientProtocol::TagSelection& tagwl, std::string& line);

	inline static void AppendUTF8(const std::string& in, std::string& out)
	{
		utf8::unchecked::replace_invalid(in.begin(), in.end(), std::back_inserter(out));
	}

	inline static size_t TruncateUTF8(const std::string& str, size_t len)
	{
		if (str.length() < len)
			return str.length();

		// Skip back to the start of the previous codepoint.
		while (len && (static_cast<unsigned char>(str[len]) & 0xC0) == 0x80)
			len--;
		return len;
	}

public:
	UTF8Serializer(const WeakModulePtr& mod)
		: ClientProtocol::Serializer(mod, "utf8")
	{
	}

	bool Parse(LocalUser* user, const std::string& line, ClientProtocol::ParseOutput& parseoutput) override;
	ClientProtocol::SerializedMessage Serialize(const ClientProtocol::Message& msg, const ClientProtocol::TagSelection& tagwl) const override;
};

bool UTF8Serializer::Parse(LocalUser* user, const std::string& line, ClientProtocol::ParseOutput& parseoutput)
{
	// Work out how long the message can actually be.
	auto maxline = ServerInstance->Config->Limits.MaxLine - 2;
	if (line[0] == '@')
		maxline += MAX_CLIENT_MESSAGE_TAG_LENGTH + 1;

	MessageTokenizer tokens(line, 0, TruncateUTF8(line, maxline));
	if (!utf8::is_valid(line))
	{
		user->WriteReply(Reply::FAIL, nullptr, "INVALID_UTF8", "Message rejected, your IRC software MUST use UTF-8 encoding on this network");
		user->CommandFloodPenalty += 2000;
		return false;
	}

	// Try to read either the tags or the command name. Unlike with the RFC
	// serializer we do not allow for broken legacy clients to send preceding
	// whitespace.
	std::string token;
	tokens.GetMiddle(token);
	if (token.empty())
	{
		// Discourage the user from flooding the server.
		user->CommandFloodPenalty += 2000;
		return false;
	}

	ServerInstance->Logs.RawIO("USERINPUT", "C[{}] I {}", user->uuid, tokens.GetMessage());
	if (token[0] == '@')
	{
		// Check that the client tags fit within the client tag space.
		if (token.length() > MAX_CLIENT_MESSAGE_TAG_LENGTH)
		{
			user->WriteNumeric(ERR_INPUTTOOLONG, "Input line was too long");
			user->CommandFloodPenalty += 2000;
			return false;
		}

		// Truncate the RFC part of the message if it is too long.
		size_t maxrfcline = token.length() + ServerInstance->Config->Limits.MaxLine - 1;
		if (tokens.GetMessage().length() > maxrfcline)
			tokens.GetMessage().erase(TruncateUTF8(tokens.GetMessage(), maxrfcline));

		// Line begins with message tags, parse them.
		std::string tagval;
		StringSplitter ss(token, ';', true, 1);
		while (ss.GetToken(token))
		{
			// Two or more tags with the same key must not be sent, but if a client violates that we accept
			// the first occurrence of duplicate tags and ignore all later occurrences.
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

namespace
{
	void CheckTagLength(std::string& line, size_t prevsize, size_t& length, size_t maxlength)
	{
		const std::string::size_type diffsize = line.size() - prevsize;
		if (length + diffsize > maxlength)
			line.erase(prevsize);
		else
			length += diffsize;
	}
}

void UTF8Serializer::SerializeTags(const ClientProtocol::TagMap& tags, const ClientProtocol::TagSelection& tagwl, std::string& line)
{
	size_t client_tag_length = 0;
	size_t server_tag_length = 0;
	for (ClientProtocol::TagMap::const_iterator i = tags.begin(); i != tags.end(); ++i)
	{
		if (!tagwl.IsSelected(tags, i))
			continue;

		const auto prevsize = line.size();
		line.push_back(prevsize ? ';' : '@');
		AppendUTF8(i->first, line);
		const auto& val = i->second.value;
		if (!val.empty())
		{
			line.push_back('=');
			AppendUTF8(val, line);
		}

		// The tags part of the message must not contain more client and server tags than allowed by the
		// message tags specification. This is complicated by the tag space having separate limits for
		// both server-originated and client-originated tags. If either of the tag limits is exceeded then
		// the most recently added tag is removed.
		if (i->first[0] == '+')
			CheckTagLength(line, prevsize, client_tag_length, MAX_CLIENT_MESSAGE_TAG_LENGTH);
		else
			CheckTagLength(line, prevsize, server_tag_length, MAX_SERVER_MESSAGE_TAG_LENGTH);
	}

	if (!line.empty())
		line.push_back(' ');
}

ClientProtocol::SerializedMessage UTF8Serializer::Serialize(const ClientProtocol::Message& msg, const ClientProtocol::TagSelection& tagwl) const
{
	std::string line;
	SerializeTags(msg.GetTags(), tagwl, line);

	// Save position for length calculation later
	const auto rfcmsg_begin = line.size();

	if (msg.GetSource())
	{
		line.push_back(':');
		AppendUTF8(*msg.GetSource(), line);
		line.push_back(' ');
	}

	AppendUTF8(msg.GetCommand(), line);

	const auto& params = msg.GetParams();
	if (!params.empty())
	{
		for (auto param = params.begin(); param != params.end() - 1; ++param)
		{
			line.push_back(' ');
			AppendUTF8(*param, line);
		}

		line.append(" :", 2);
		AppendUTF8(params.back(), line);
	}

	// Truncate if too long
	auto maxline = ServerInstance->Config->Limits.MaxLine - 2;
	if (line.length() - rfcmsg_begin > maxline)
		line.erase(TruncateUTF8(line, rfcmsg_begin + maxline));

	line.append("\r\n", 2);
	return line;
}

class IRCv3UTF8Only final
	: public Module
	, public ISupport::EventListener
{
private:
	UTF8Serializer utf8serializer;

public:
	IRCv3UTF8Only()
		: Module(VF_CORE | VF_VENDOR, "Provides support for the IRCv3 UTF8ONLY specification.")
		, ISupport::EventListener(weak_from_this())
		, utf8serializer(weak_from_this())
	{
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(shared_from_this(), I_OnUserInit, PRIORITY_BEFORE, "core_serialize_rfc");
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["CHARSET"] = "utf8";
		tokens["UTF8ONLY"];
	}

	void OnCleanup(ExtensionType type, Extensible* item) override
	{
		if (type != ExtensionType::USER)
			return;

		auto* const user = static_cast<User*>(item)->AsLocal();
		if (user && user->serializer == &utf8serializer)
			ServerInstance->Users.QuitUser(user, "Protocol serializer module unloading");
	}

	void OnUserInit(LocalUser* user) override
	{
		if (!user->serializer)
			user->serializer = &utf8serializer;
	}
};

MODULE_INIT(IRCv3UTF8Only)
