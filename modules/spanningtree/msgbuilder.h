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

class MessageBuilder
{
protected:
	// The command name.
	std::string command;

	// Whether this message has been finalized yet.
	bool finalized = false;

	// Parameters to to the command.
	CommandBase::Params parameters;

	// The user that the message is from, or nullptr for no source.
	User* source;

	// Finalizes the message by calling events etc.
	void Finalize();

public:
	MessageBuilder(const std::string& cmd, bool nosource = false)
		: command(cmd)
		, source(nosource ? nullptr : ServerInstance->FakeClient)
	{
	}

	MessageBuilder(const TreeServer* src, const std::string& cmd) ATTR_NOT_NULL(2)
		: command(cmd)
		, source(src->ServerUser)
	{
	}

	MessageBuilder(const User* src, const std::string& cmd) ATTR_NOT_NULL(2)
		: command(cmd)
		, source(const_cast<User*>(src))
	{
	}

	// Broadcast this message to the entire network.
	void Broadcast(const TreeServer* omit = nullptr);

	// Retrieves the command name.
	inline auto& GetCommand() { return this->command; }

	// Retrieves the parameters to the command.
	inline auto& GetParameters() { return this->parameters; }

	// Retrieves the user that the message is from, or nullptr for no source.
	inline auto*& GetSource() { return this->source; }

	// Adds a parameter to the parameter list.
	template <typename... Args>
	MessageBuilder& Push(Args&&... args)
	{
		(this->parameters.push_back(ConvToStr(args)), ...);
		return *this;
	}

	// Formats a parameter and adds it to the parameter list.
	template <typename... Args>
	MessageBuilder& PushFmt(const char* text, Args&&... args)
	{
		return Push(FMT::vformat(text, FMT::make_format_args(args...)));
	}

	// Adds a vector of command parameters to the parameter list.
	MessageBuilder& PushParams(const CommandBase::Params& newparams);

	// Adds tags to the tag list.
	MessageBuilder& PushTags(ClientProtocol::TagMap newtags);

	// Converts this message to the RFC 1459 form.
	std::string ToRFC1459() const;

	// Unicast this message to the specified server.
	void Unicast(TreeServer* server) ATTR_NOT_NULL(2);

	// Unicast this message to the specified socket.
	void Unicast(TreeSocket* socket) ATTR_NOT_NULL(2);

	// Unicast this message to the server of the specified user.
	void Unicast(const User* user) ATTR_NOT_NULL(2);
};
