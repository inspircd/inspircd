/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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

/** Whether message was a PRIVMSG or a NOTICE. */
enum MessageType
{
	/** The message is a PRIVMSG. */
	MSG_PRIVMSG,

	/** The message is a NOTICE. */
	MSG_NOTICE
};

class CoreExport MessageDetails
{
 public:
	/* Whether to send the original message back to clients with echo-message support. */
	bool echooriginal;

	 /** The users who are exempted from receiving this message. */
	CUList exemptions;

	/* The original message as sent by the user. */
	const std::string originaltext;

	/** The message which will be sent to clients. */
	std::string text;

	/** The type of message. */
	const MessageType type;

	MessageDetails(MessageType mt, const std::string& msg)
		: echooriginal(false)
		, originaltext(msg)
		, text(msg)
		, type(mt)
	{
	}
};

/** Represents the target of a message (NOTICE, PRIVMSG, etc). */
class CoreExport MessageTarget
{
 public:
	/** An enumeration of possible message target types. */
	enum TargetType
	{
		/** The target of the message is a user. */
		TYPE_USER,

		/** The target of the message is a channel. */
		TYPE_CHANNEL,

		/** The target of the message is a server. */
		TYPE_SERVER
	};

 private:
	/** The target of the message. */
	void* dest;

 public:
	/** If type is TYPE_CHANNEL and the user specified a status rank. */
	char status;

	/** The type of the target of the message. If this is TYPE_CHANNEL then dest
	 * is a Channel*, TYPE_USER then dest is a User*, and TYPE_SERVER then dest is
	 * a std::string* containing a server glob.
	 */
	MessageTarget::TargetType type;

	/** Initialises a new channel message target.
	 * @param channel The channel which is the target of the message.
	 * @param statuschar The lowest status rank that the message is being sent to.
	 */
	MessageTarget(Channel* channel, char statuschar)
		: dest(channel)
		, status(statuschar)
		, type(TYPE_CHANNEL)
	{
	}

	/** Initialises a new user message target.
	 * @param user The user which is the target of the message.
	 */
	MessageTarget(User* user)
		: dest(user)
		, status(0)
		, type(TYPE_USER)
	{
	}

	/** Initialises a new server message target.
	 * @param server The server glob which is the target of the message.
	 */
	MessageTarget(std::string* server)
		: dest(server)
		, status(0)
		, type(TYPE_SERVER)
	{
	}

	/** Retrieves the target of this message. */
	template<typename T>
	T* Get() const
	{
		return static_cast<T*>(dest);
	}
};
