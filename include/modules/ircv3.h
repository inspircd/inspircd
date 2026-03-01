/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015, 2018 Attila Molnar <attilamolnar@hush.com>
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

#include "modules/cap.h"

namespace IRCv3
{
	class WriteNeighborsWithCap;
	template <typename T>
	class CapTag;
	class ReplyCapReference;

	/**
	 * Sends a standard reply to the specified user if they have the specified cap
	 * or a notice if they do not.
	 * @param user The user to send the reply to.
	 * @param cap The capability that determines the type of message to send.
	 * @param command The command that the reply relates to.
	 * @param code A machine readable code for this reply.
	 * @param args A variable number of context parameters and a human readable description of this reply.
	 */
	template<typename... Args>
	void WriteReply(Reply::Type rt, User* user, const Cap::Capability* cap, const Command* command,
		const std::string& code, Args&&... args)
	{
		static_assert(sizeof...(Args) >= 1);

		if (cap && cap->IsEnabled(user))
			user->WriteRemoteReply(rt, command, code, std::forward<Args>(args)...);
		else if (command)
			user->WriteRemoteNotice("*** {}: {}", command->name, std::get<sizeof...(Args) - 1>(std::forward_as_tuple(args...)));
		else
			user->WriteRemoteNotice("*** {}", std::get<sizeof...(Args) - 1>(std::forward_as_tuple(args...)));
	}
}

class IRCv3::WriteNeighborsWithCap final
	: public User::ForEachNeighborHandler
{
private:
	const Cap::Capability& cap;
	ClientProtocol::Event& protoev;
	uint64_t sentid;

	void Execute(LocalUser* user) override
	{
		if (cap.IsEnabled(user))
			user->Send(protoev);
	}

public:
	WriteNeighborsWithCap(User* user, ClientProtocol::Event& ev, const Cap::Capability& capability, bool include_self = false)
		: cap(capability)
		, protoev(ev)
	{
		sentid = user->ForEachNeighbor(*this, include_self);
	}

	uint64_t GetAlreadySentId() const { return sentid; }
};

/** Base class for simple message tags.
 * Message tags provided by classes derived from this class will be sent to clients that have negotiated
 * a client capability, also managed by this class.
 *
 * Derived classes specify the name of the capability and the message tag and provide a public GetValue()
 * method with the following signature: const std::string* GetValue(ClientProtocol::Message& msg).
 * The returned value determines whether to attach the tag to the message. If it is NULL, the tag won't
 * be attached. If it is non-NULL the tag will be attached with the value in the string. If the string is
 * empty the tag is attached without a value.
 *
 * Providers inheriting from this class don't accept incoming tags by default.
 *
 * For more control, inherit from ClientProtocol::MessageTagProvider directly.
 *
 * Template parameter T is the derived class.
 */
template <typename T>
class IRCv3::CapTag
	: public ClientProtocol::MessageTagProvider
{
protected:
	Cap::Capability cap;
	const std::string tagname;

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) override
	{
		return cap.IsEnabled(user);
	}

	void OnPopulateTags(ClientProtocol::Message& msg) override
	{
		T& tag = static_cast<T&>(*this);
		const std::string* const val = tag.GetValue(msg);
		if (val)
			msg.AddTag(tagname, this, *val);
	}

public:
	/** Constructor.
	 * @param mod Module that owns the tag.
	 * @param capname Name of the client capability.
	 * A client capability with this name will be created. It will be available to all clients and it won't
	 * have a value.
	 * See Cap::Capability for more info on client capabilities.
	 * @param Tagname Name of the message tag, to use in the protocol.
	 */
	CapTag(Module* mod, const std::string& capname, const std::string& Tagname)
		: ClientProtocol::MessageTagProvider(mod)
		, cap(mod, capname)
		, tagname(Tagname)
	{
	}

	/** Retrieves the underlying capability. */
	const Cap::Capability& GetCap() const { return cap; }
};

/** Reference to the standard-replies cap. */
class IRCv3::ReplyCapReference final
	: public Cap::Reference
{
public:
	ReplyCapReference(Module* mod)
		: Cap::Reference(mod, "standard-replies")
	{
	}
};
