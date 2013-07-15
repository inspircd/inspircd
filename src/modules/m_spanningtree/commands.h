/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

#include "main.h"

/** Handle /RCONNECT
 */
class CommandRConnect : public Command
{
 public:
        CommandRConnect(Module* Creator);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandRSQuit : public Command
{
 public:
        CommandRSQuit(Module* Creator);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandSVSJoin : public ServerCommand
{
 public:
	CommandSVSJoin(Module* Creator) : ServerCommand(Creator, "SVSJOIN", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandSVSPart : public ServerCommand
{
 public:
	CommandSVSPart(Module* Creator) : ServerCommand(Creator, "SVSPART", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandSVSNick : public ServerCommand
{
 public:
	CommandSVSNick(Module* Creator) : ServerCommand(Creator, "SVSNICK", 3) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandMetadata : public ServerCommand
{
 public:
	CommandMetadata(Module* Creator) : ServerCommand(Creator, "METADATA", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandUID : public ServerCommand
{
 public:
	CommandUID(Module* Creator) : ServerCommand(Creator, "UID", 10) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandOpertype : public ServerCommand
{
 public:
	CommandOpertype(Module* Creator) : ServerCommand(Creator, "OPERTYPE", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class TreeSocket;
class CommandFJoin : public ServerCommand
{
	/** Remove all modes from a channel, including statusmodes (+qaovh etc), simplemodes, parameter modes.
	 * This does not update the timestamp of the target channel, this must be done seperately.
	 */
	static void RemoveStatus(Channel* c);
	static void ApplyModeStack(User* srcuser, Channel* c, irc::modestacker& stack);
	bool ProcessModeUUIDPair(const std::string& item, TreeSocket* src_socket, Channel* chan, irc::modestacker* modestack);
 public:
	CommandFJoin(Module* Creator) : ServerCommand(Creator, "FJOIN", 3) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandFMode : public ServerCommand
{
 public:
	CommandFMode(Module* Creator) : ServerCommand(Creator, "FMODE", 3) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandFTopic : public ServerCommand
{
 public:
	CommandFTopic(Module* Creator) : ServerCommand(Creator, "FTOPIC", 5) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandFHost : public ServerCommand
{
 public:
	CommandFHost(Module* Creator) : ServerCommand(Creator, "FHOST", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandFIdent : public ServerCommand
{
 public:
	CommandFIdent(Module* Creator) : ServerCommand(Creator, "FIDENT", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandFName : public ServerCommand
{
 public:
	CommandFName(Module* Creator) : ServerCommand(Creator, "FNAME", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandIJoin : public ServerCommand
{
 public:
	CommandIJoin(Module* Creator) : ServerCommand(Creator, "IJOIN", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
};

class CommandResync : public ServerCommand
{
 public:
	CommandResync(Module* Creator) : ServerCommand(Creator, "RESYNC", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandAway : public ServerCommand
{
 public:
	CommandAway(Module* Creator) : ServerCommand(Creator, "AWAY", 0, 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandAddLine : public ServerCommand
{
 public:
	CommandAddLine(Module* Creator) : ServerCommand(Creator, "ADDLINE", 6, 6) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandDelLine : public ServerCommand
{
 public:
	CommandDelLine(Module* Creator) : ServerCommand(Creator, "DELLINE", 2, 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandEncap : public ServerCommand
{
 public:
	CommandEncap(Module* Creator) : ServerCommand(Creator, "ENCAP", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandIdle : public ServerCommand
{
 public:
	CommandIdle(Module* Creator) : ServerCommand(Creator, "IDLE", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class CommandNick : public ServerCommand
{
 public:
	CommandNick(Module* Creator) : ServerCommand(Creator, "NICK", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandPing : public ServerCommand
{
 public:
	CommandPing(Module* Creator) : ServerCommand(Creator, "PING", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class CommandPong : public ServerCommand
{
 public:
	CommandPong(Module* Creator) : ServerCommand(Creator, "PONG", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class CommandPush : public ServerCommand
{
 public:
	CommandPush(Module* Creator) : ServerCommand(Creator, "PUSH", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class CommandSave : public ServerCommand
{
 public:
	CommandSave(Module* Creator) : ServerCommand(Creator, "SAVE", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandServer : public ServerCommand
{
 public:
	CommandServer(Module* Creator) : ServerCommand(Creator, "SERVER", 5) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandSQuit : public ServerCommand
{
 public:
	CommandSQuit(Module* Creator) : ServerCommand(Creator, "SQUIT", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandSNONotice : public ServerCommand
{
 public:
	CommandSNONotice(Module* Creator) : ServerCommand(Creator, "SNONOTICE", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandVersion : public ServerCommand
{
 public:
	CommandVersion(Module* Creator) : ServerCommand(Creator, "VERSION", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandBurst : public ServerCommand
{
 public:
	CommandBurst(Module* Creator) : ServerCommand(Creator, "BURST") { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandEndBurst : public ServerCommand
{
 public:
	CommandEndBurst(Module* Creator) : ServerCommand(Creator, "ENDBURST") { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class SpanningTreeCommands
{
 public:
	CommandRConnect rconnect;
	CommandRSQuit rsquit;
	CommandSVSJoin svsjoin;
	CommandSVSPart svspart;
	CommandSVSNick svsnick;
	CommandMetadata metadata;
	CommandUID uid;
	CommandOpertype opertype;
	CommandFJoin fjoin;
	CommandIJoin ijoin;
	CommandResync resync;
	CommandFMode fmode;
	CommandFTopic ftopic;
	CommandFHost fhost;
	CommandFIdent fident;
	CommandFName fname;
	CommandAway away;
	CommandAddLine addline;
	CommandDelLine delline;
	CommandEncap encap;
	CommandIdle idle;
	CommandNick nick;
	CommandPing ping;
	CommandPong pong;
	CommandPush push;
	CommandSave save;
	CommandServer server;
	CommandSQuit squit;
	CommandSNONotice snonotice;
	CommandVersion version;
	CommandBurst burst;
	CommandEndBurst endburst;
	SpanningTreeCommands(ModuleSpanningTree* module);
};
