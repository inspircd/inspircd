/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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
}

class IRCv3::WriteNeighborsWithCap : public User::ForEachNeighborHandler
{
	const Cap::Capability& cap;
	ClientProtocol::Event& protoev;

	void Execute(LocalUser* user) CXX11_OVERRIDE
	{
		if (cap.get(user))
			user->Send(protoev);
	}

 public:
	WriteNeighborsWithCap(User* user, ClientProtocol::Event& ev, const Cap::Capability& capability)
		: cap(capability)
		, protoev(ev)
	{
		user->ForEachNeighbor(*this, false);
	}
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
class IRCv3::CapTag : public ClientProtocol::MessageTagProvider
{
	Cap::Capability cap;
	const std::string tagname;

	bool ShouldSendTag(LocalUser* user, const ClientProtocol::MessageTagData& tagdata) CXX11_OVERRIDE
	{
		return cap.get(user);
	}

	void OnClientProtocolPopulateTags(ClientProtocol::Message& msg) CXX11_OVERRIDE
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
};
