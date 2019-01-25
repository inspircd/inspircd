/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

class SpanningTreeProtocolInterface : public ProtocolInterface
{
 public:
	class Server : public ProtocolInterface::Server
	{
		TreeSocket* const sock;

	 public:
		Server(TreeSocket* s) : sock(s) { }
		void SendMetaData(const std::string& key, const std::string& data) override;
	};

	bool SendEncapsulatedData(const std::string& targetmask, const std::string& cmd, const CommandBase::Params& params, User* source) override;
	void BroadcastEncap(const std::string& cmd, const CommandBase::Params& params, User* source, User* omit) override;
	void SendMetaData(User* user, const std::string& key, const std::string& data) override;
	void SendMetaData(Channel* chan, const std::string& key, const std::string& data) override;
	void SendMetaData(const std::string& key, const std::string& data) override;
	void SendSNONotice(char snomask, const std::string& text) override;
	void SendMessage(Channel* target, char status, const std::string& text, MessageType msgtype) override;
	void SendMessage(User* target, const std::string& text, MessageType msgtype) override;
	void GetServerList(ServerList& sl) override;
};
