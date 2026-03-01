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


#pragma once

namespace Reply
{
	class Reply;

	enum class Type
	{
		FAIL,
		WARN,
		NOTE,
	};

	/** Get command name from Reply::Type.
	 * @param mt Reply type to get command name for.
	 * @return Command name for the message type.
	 */
	inline const char* CommandStrFromType(Type rt);
}

class Reply::Reply
{
private:
	/** The machine-readable code. */
	const std::string code;

	/** The command the reply is associated with or nullptr for no command. */
	const Command* command = nullptr;

	/** The parameters of the reply. */
	CommandBase::Params params;

	/** The source of the reply or nullptr for the local server. */
	Server* source = nullptr;

	/** The type of reply we are sending. */
	const ::Reply::Type type;

public:
	Reply(::Reply::Type rt, const Command* cmd, const std::string& c)
		: code(c)
		, command(cmd)
		, type(rt)
	{
	}

	/** Add a tag to the reply.
	 * @param tagname The name of the tag to use.
	 * @param tagprov The provider of the tag.
	 * @param tagvalue If non-empty then the value of the tag.
	 * @param tagdata If non-null then ata specific to the tag provider to be passed to MessageTagProvider::ShouldSendTag().
	 */
	Reply& AddTag(const std::string& tagname, ClientProtocol::MessageTagProvider* tagprov, const std::string& tagvalue, void* tagdata = nullptr)
	{
		this->params.GetTags().emplace(tagname, ClientProtocol::MessageTagData(tagprov, tagvalue, tagdata));
		return *this;
	}

	/** Add all tags in a TagMap to the tags in this message. Existing tags will not be overwritten.
	 * @param newtags New tags to add.
	 */
	Reply& AddTags(const ClientProtocol::TagMap& newtags)
	{
		this->params.GetTags().insert(newtags.begin(), newtags.end());
		return *this;
	}

	/** Retrieves the machine readable code. */
	const auto& GetCode() const { return this->code; }

	/** Retrieves the command the reply is associated with or nullptr for no command. */
	auto* GetCommand() const { return this->command; }

	/** Retrieves the parameters of the reply. */
	const auto& GetParams() const { return params; }
	auto& GetParams() { return params; }

	/** Retrieves the source server of the reply. */
	auto* GetSource() const { return this->source; }

	/** Retrieves the type of the reply. */
	auto GetType() const { return this->type; }

	/** Converts the given arguments to a string and adds them to the numeric.
	 * @param args One or more arguments to the numeric.
	 */
	template <typename... Args>
	Reply& PushParam(Args&&... args)
	{
		(params.push_back(ConvToStr(args)), ...);
		return *this;
	}

	/** Formats the string with the specified arguments and adds them to the numeric.
	 * @param text A format string to format and then push.
	 * @param args One or more arguments to format the string with.
	 */
	template <typename... Args>
	Reply& PushParamFmt(const char* text, Args&&... args)
	{
		this->PushParam(FMT::vformat(text, FMT::make_format_args(args...)));
		return *this;
	}

	/** Set the source server of the reply.
	 * @param s The server to set as the source.
	 */
	void SetSource(Server* s) { this->source = s; }
};

const char* Reply::CommandStrFromType(Type rt)
{
	switch (rt)
	{
		case Type::FAIL:
			return "FAIL";
		case Type::WARN:
			return "WARN";
		case Type::NOTE:
			return "NOTE";
	}
	return nullptr; // Should never happen.
}
