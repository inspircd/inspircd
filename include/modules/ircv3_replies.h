/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
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
	namespace Replies
	{
		class CapReference;
		class Reply;
		class Fail;
		class Note;
		class Warn;
	}
}

/** Reference to the standard-replies cap. */
class IRCv3::Replies::CapReference final
	: public Cap::Reference
{
public:
	CapReference(Module* mod)
		: Cap::Reference(mod, "standard-replies")
	{
	}
};

/** Base class for standard replies. */
class IRCv3::Replies::Reply
{
private:
	/** The name of the command for this reply. */
	const std::string cmd;

	/** The event provider for this reply. */
	ClientProtocol::EventProvider evprov;

	/** Wraps a message in an event and sends it to a user.
	 * @param user The user to send the message to.
	 * @param msg The message to send to the user.
	 */
	void SendInternal(LocalUser* user, ClientProtocol::Message& msg)
	{
		ClientProtocol::Event ev(evprov, msg);
		user->Send(ev);
	}

	void SendNoticeInternal(LocalUser* user, const Command* command, const std::string& description)
	{
		if (command)
			user->WriteNotice("*** {}: {}", command->name, description);
		else
			user->WriteNotice("*** {}", description);
	}

protected:
	/** Initializes a new instance of the Reply class.
	 * @param Creator The module which created this instance.
	 * @param Cmd The name of the command to reply with.
	 */
	Reply(Module* Creator, const std::string& Cmd)
		: cmd(Cmd)
		, evprov(Creator, Cmd)
	{
	}

public:
	/**
	 * Sends a standard reply to the specified user.
	 * @param user The user to send the reply to.
	 * @param command The command that the reply relates to.
	 * @param code A machine readable code for this reply.
	 * @param args A variable number of context parameters and a human readable description of this reply.
	 */
	template<typename... Args>
	void Send(LocalUser* user, const Command* command, const std::string& code, Args&&... args)
	{
		static_assert(sizeof...(Args) >= 1);

		ClientProtocol::Message msg(cmd.c_str(), ServerInstance->Config->GetServerName());
		if (command)
			msg.PushParamRef(command->name);
		else
			msg.PushParam("*");
		msg.PushParam(code);
		msg.PushParam(std::forward<Args>(args)...);
		SendInternal(user, msg);
	}

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
	void SendIfCap(LocalUser* user, const Cap::Capability* cap, const Command* command, const std::string& code,
		Args&&... args)
	{
		static_assert(sizeof...(Args) >= 1);

		if (cap && cap->IsEnabled(user))
			Send(user, command, code, std::forward<Args>(args)...);
		else
			SendNoticeInternal(user, command, std::get<sizeof...(Args) - 1>(std::forward_as_tuple(args...)));
	}
};

/** Sends a FAIL standard reply. */
class IRCv3::Replies::Fail final
	: public IRCv3::Replies::Reply
{
public:
	/** Initializes a new instance of the Fail class.
	 * @param Creator The module which created this instance.
	 */
	Fail(Module* Creator)
		: Reply(Creator, "FAIL")
	{
	}
};

/** Sends a NOTE standard reply. */
class IRCv3::Replies::Note final
	: public IRCv3::Replies::Reply
{
public:
	/** Initializes a new instance of the Note class.
	 * @param Creator The module which created this instance.
	 */
	Note(Module* Creator)
		: Reply(Creator, "NOTE")
	{
	}
};

/** Sends a WARN standard reply. */
class IRCv3::Replies::Warn final
	: public IRCv3::Replies::Reply
{
public:
	/** Initializes a new instance of the Warn class.
	 * @param Creator The module which created this instance.
	 */
	Warn(Module* Creator)
		: Reply(Creator, "WARN")
	{
	}
};
