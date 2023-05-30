/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class SpanningTreeProtocolInterface final
	: public ProtocolInterface
{
public:
	class Server final
		: public ProtocolInterface::Server
	{
		TreeSocket* const sock;

	public:
		Server(TreeSocket* s)
			: sock(s)
		{
		}
		void SendMetadata(const std::string& key, const std::string& data) override;
	};

	bool SendEncapsulatedData(const std::string& targetmask, const std::string& cmd, const CommandBase::Params& params, const User* source) override;
	void BroadcastEncap(const std::string& cmd, const CommandBase::Params& params, const User* source, const User* omit) override;
	void SendMetadata(const Extensible* ext, const std::string& key, const std::string& data) override;
	void SendMetadata(const std::string& key, const std::string& data) override;
	void SendSNONotice(char snomask, const std::string& text) override;
	void SendMessage(const Channel* target, char status, const std::string& text, MessageType msgtype) override;
	void SendMessage(const User* target, const std::string& text, MessageType msgtype) override;
	void GetServerList(ServerList& sl) override;
};
