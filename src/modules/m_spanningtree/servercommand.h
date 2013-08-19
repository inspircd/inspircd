/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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

class TreeServer;

/** Base class for server-to-server commands that may have a (remote) user source or server source.
 */
class ServerCommand : public CommandBase
{
 public:
	ServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0);

	virtual CmdResult Handle(User* user, std::vector<std::string>& parameters) = 0;
	virtual RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

/** Base class for server-to-server command handlers which are only valid if their source is a user.
 * When a server sends a command of this type and the source is a server (sid), the link is aborted.
 */
template <class T>
class UserOnlyServerCommand : public ServerCommand
{
 public:
	UserOnlyServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0)
		: ServerCommand(Creator, Name, MinPara, MaxPara) { }

	CmdResult Handle(User* user, std::vector<std::string>& parameters)
    {
    	RemoteUser* remoteuser = IS_REMOTE(user);
		if (!remoteuser)
			return CMD_INVALID;
		return static_cast<T*>(this)->HandleRemote(remoteuser, parameters);
    }
};

/** Base class for server-to-server command handlers which are only valid if their source is a server.
 * When a server sends a command of this type and the source is a user (uuid), the link is aborted.
 */
template <class T>
class ServerOnlyServerCommand : public ServerCommand
{
 public:
	ServerOnlyServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0)
		: ServerCommand(Creator, Name, MinPara, MaxPara) { }

	CmdResult Handle(User* user, std::vector<std::string>& parameters)
    {
		if (!IS_SERVER(user))
			return CMD_INVALID;
		TreeServer* server = Utils->FindServer(user->server);
		return static_cast<T*>(this)->HandleServer(server, parameters);
    }
};

class ServerCommandManager
{
	typedef TR1NS::unordered_map<std::string, ServerCommand*> ServerCommandMap;
	ServerCommandMap commands;

 public:
	ServerCommand* GetHandler(const std::string& command) const;
	bool AddCommand(ServerCommand* cmd);
};
