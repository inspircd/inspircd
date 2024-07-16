/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
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

#include "utils.h"
#include "treeserver.h"

class ProtocolException final
	: public ModuleException
{
public:
	ProtocolException(const std::string& msg)
		: ModuleException((Module*)Utils->Creator, "Protocol violation: " + msg)
	{
	}
};

/** Base class for server-to-server commands that may have a (remote) user source or server source.
 */
class ServerCommand
	: public CommandBase
{
public:
	ServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0);

	/** Register this object in the ServerCommandManager
	 */
	void RegisterService() override;

	virtual CmdResult Handle(User* user, Params& parameters) = 0;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;

	/**
	 * Extract the TS from a string.
	 * @param tsstr The string containing the TS.
	 * @return The raw timestamp value.
	 * This function throws a ProtocolException if it considers the TS invalid. Note that the detection of
	 * invalid timestamps is not designed to be bulletproof, only some cases - like "0" - trigger an exception.
	 */
	static time_t ExtractTS(const std::string& tsstr);
};

/** Base class for server-to-server command handlers which are only valid if their source is a user.
 * When a server sends a command of this type and the source is a server (sid), the link is aborted.
 */
template <class T>
class UserOnlyServerCommand
	: public ServerCommand
{
public:
	UserOnlyServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0)
		: ServerCommand(Creator, Name, MinPara, MaxPara) { }

	CmdResult Handle(User* user, Params& parameters) override
	{
		RemoteUser* remoteuser = IS_REMOTE(user);
		if (!remoteuser)
			throw ProtocolException("Invalid source");
		return static_cast<T*>(this)->HandleRemote(remoteuser, parameters);
	}
};

/** Base class for server-to-server command handlers which are only valid if their source is a server.
 * When a server sends a command of this type and the source is a user (uuid), the link is aborted.
 */
template <class T>
class ServerOnlyServerCommand
	: public ServerCommand
{
public:
	ServerOnlyServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0)
		: ServerCommand(Creator, Name, MinPara, MaxPara) { }

	CmdResult Handle(User* user, CommandBase::Params& parameters) override
	{
		if (!IS_SERVER(user))
			throw ProtocolException("Invalid source");
		TreeServer* server = TreeServer::Get(user);
		return static_cast<T*>(this)->HandleServer(server, parameters);
	}
};

class ServerCommandManager final
{
	typedef std::unordered_map<std::string, ServerCommand*> ServerCommandMap;
	ServerCommandMap commands;

public:
	ServerCommand* GetHandler(const std::string& command) const;
	bool AddCommand(ServerCommand* cmd);
};
