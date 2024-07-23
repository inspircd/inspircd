/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

class User;

[[deprecated("ProtocolServer has been merged with Server")]]
typedef ::Server ProtocolServer;

class CoreExport ProtocolInterface
{
public:
	[[deprecated("ProtocolInterface::Server has been merged with Server")]]
	typedef ProtocolServer Server;

	class ServerInfo final
	{
	public:
		std::string servername;
		std::string parentname;
		std::string description;
		size_t usercount;
		size_t opercount;
		unsigned long latencyms;
	};

	typedef std::vector<ServerInfo> ServerList;

	virtual ~ProtocolInterface() = default;

	/** Send an ENCAP message to all servers matching a wildcard string.
	 * See the protocol documentation for the purpose of ENCAP.
	 * @param targetmask The target server mask (can contain wildcards)
	 * @param cmd The ENCAP subcommand
	 * @param params List of string parameters which are dependent on the subcommand
	 * @param source The source of the message (prefix), must be a local user or NULL which means use local server
	 * @return Always true if the target mask contains wildcards; otherwise true if the server name was found,
	 * and the message was sent, false if it was not found.
	 * ENCAP (should) be used instead of creating new protocol messages for easier third party application support.
	 */
	virtual bool SendEncapsulatedData(const std::string& targetmask, const std::string& cmd, const CommandBase::Params& params, const User* source = nullptr) { return false; }

	/** Send an ENCAP message to all servers.
	 * See the protocol documentation for the purpose of ENCAP.
	 * @param cmd The ENCAP subcommand
	 * @param params List of string parameters which are dependent on the subcommand
	 * @param source The source of the message (prefix), must be a local user or a user behind 'omit'
	 * or NULL which is equivalent to the local server
	 * @param omit If non-NULL the message won't be sent in the direction of this server, useful for forwarding messages
	 */
	virtual void BroadcastEncap(const std::string& cmd, const CommandBase::Params& params, const User* source = nullptr, const User* omit = nullptr) { }

	/** Send metadata for an extensible to other linked servers.
	 * @param ext The extensible to send metadata for
	 * @param key The 'key' of the data, e.g. "swhois" for swhois desc on a user
	 * @param data The string representation of the data
	 */
	virtual void SendMetadata(const Extensible* ext, const std::string& key, const std::string& data) { }

	/** Send metadata related to the server to other linked servers.
	 * @param key The 'key' of the data
	 * @param data The string representation of the data
	 */
	virtual void SendMetadata(const std::string& key, const std::string& data) { }

	/** Send a notice to users with a given snomask.
	 * @param snomask The snomask required for the message to be sent.
	 * @param text The message to send.
	 */
	virtual void SendSNONotice(char snomask, const std::string& text) { }

	/** Send a message to a channel.
	 * @param target The channel to message.
	 * @param status The status character (e.g. %) required to receive.
	 * @param text The message to send.
	 * @param type The message type (MessageType::PRIVMSG or MessageType::NOTICE)
	 */
	virtual void SendMessage(const Channel* target, char status, const std::string& text, MessageType type = MessageType::PRIVMSG) { }

	/** Send a message to a user.
	 * @param target The user to message.
	 * @param text The message to send.
	 * @param type The message type (MessageType::PRIVMSG or MessageType::NOTICE)
	 */
	virtual void SendMessage(const User* target, const std::string& text, MessageType type = MessageType::PRIVMSG) { }

	/** Fill a list of servers and information about them.
	 * @param sl The list of servers to fill.
	 */
	virtual void GetServerList(ServerList& sl) { }
};
