/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Peter Powell <petpow@saberuk.com>
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

#include "event.h"

namespace CTCTags
{
	class EventListener;
	class TagMessage;
	class TagMessageDetails;
}

class CTCTags::TagMessage : public ClientProtocol::Message
{
 public:
	TagMessage(User* source, const Channel* targetchan, const ClientProtocol::TagMap& Tags)
		: ClientProtocol::Message("TAGMSG", source)
	{
		PushParamRef(targetchan->name);
		AddTags(Tags);
	}

	TagMessage(User* source, const User* targetuser, const ClientProtocol::TagMap& Tags)
		: ClientProtocol::Message("TAGMSG", source)
	{
		if (targetuser->registered & REG_NICK)
			PushParamRef(targetuser->nick);
		else
			PushParam("*");
		AddTags(Tags);
	}

	TagMessage(User* source, const char* targetstr, const ClientProtocol::TagMap& Tags)
		: ClientProtocol::Message("TAGMSG", source)
	{
		PushParam(targetstr);
		AddTags(Tags);
	}
};

class CTCTags::TagMessageDetails
{
 public:
	/** Whether to echo the tags at all. */
	bool echo;

	/* Whether to send the original tags back to clients with echo-message support. */
	bool echo_original;

	/** The users who are exempted from receiving this message. */
	CUList exemptions;

	/** IRCv3 message tags sent to the server by the user. */
	const ClientProtocol::TagMap tags_in;

	/** IRCv3 message tags sent out to users who get this message. */
	ClientProtocol::TagMap tags_out;

	TagMessageDetails(const ClientProtocol::TagMap& tags)
		: echo(true)
		, echo_original(false)
		, tags_in(tags)
	{
	}
};

class CTCTags::EventListener
	: public Events::ModuleEventListener
{
 protected:
	EventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/tagmsg", eventprio)
	{
	}

 public:
	/** Called before a user sends a tag message to a channel, a user, or a server glob mask.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message tags or whether to echo. See the
	 *                TagMessageDetails class for more information.
	 * @return MOD_RES_ALLOW to explicitly allow the message, MOD_RES_DENY to explicitly deny the
	 *         message, or MOD_RES_PASSTHRU to let another module handle the event.
	 */
	virtual ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, TagMessageDetails& details) { return MOD_RES_PASSTHRU; }
	
	/** Called immediately after a user sends a tag message to a channel, a user, or a server glob mask.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message tags or whether to echo. See the
	 *                TagMessageDetails class for more information.
	 */
	virtual void OnUserPostTagMessage(User* user, const MessageTarget& target, const TagMessageDetails& details) { }

	/** Called immediately before a user sends a tag message to a channel, a user, or a server glob mask.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message tags or whether to echo. See the
	 *                TagMessageDetails class for more information.
	 */
	virtual void OnUserTagMessage(User* user, const MessageTarget& target, const TagMessageDetails& details) { }

	/** Called when a tag message sent by a user to a channel, a user, or a server glob mask is blocked.
	 * @param user The user sending the message.
	 * @param target The target of the message. This can either be a channel, a user, or a server
	 *               glob mask.
	 * @param details Details about the message such as the message tags or whether to echo. See the
	 *                TagMessageDetails class for more information.
	 */
	virtual void OnUserTagMessageBlocked(User* user, const MessageTarget& target, const TagMessageDetails& details) { }
};
