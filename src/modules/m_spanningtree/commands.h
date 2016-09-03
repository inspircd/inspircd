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

#include "servercommand.h"
#include "commandbuilder.h"
#include "remoteuser.h"

namespace SpanningTree
{
	class CommandAway;
	class CommandNick;
	class CommandPing;
	class CommandPong;
	class CommandServer;
}

using SpanningTree::CommandAway;
using SpanningTree::CommandNick;
using SpanningTree::CommandPing;
using SpanningTree::CommandPong;
using SpanningTree::CommandServer;

/** Handle /RCONNECT
 */
class CommandRConnect : public Command
{
 public:
	CommandRConnect(Module* Creator);
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandRSQuit : public Command
{
 public:
	CommandRSQuit(Module* Creator);
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandMap : public Command
{
 public:
	CommandMap(Module* Creator);
	CmdResult Handle(const std::vector<std::string>& parameters, User* user);
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

	class Builder : public CmdBuilder
	{
	 public:
		Builder(User* user, const std::string& key, const std::string& val);
		Builder(Channel* chan, const std::string& key, const std::string& val);
		Builder(const std::string& key, const std::string& val);
	};
};

class CommandUID : public ServerOnlyServerCommand<CommandUID>
{
 public:
	CommandUID(Module* Creator) : ServerOnlyServerCommand<CommandUID>(Creator, "UID", 10) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& params);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(User* user);
	};
};

class CommandOpertype : public UserOnlyServerCommand<CommandOpertype>
{
 public:
	CommandOpertype(Module* Creator) : UserOnlyServerCommand<CommandOpertype>(Creator, "OPERTYPE", 1) { }
	CmdResult HandleRemote(RemoteUser* user, std::vector<std::string>& params);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(User* user);
	};
};

class TreeSocket;
class FwdFJoinBuilder;
class CommandFJoin : public ServerCommand
{
	/** Remove all modes from a channel, including statusmodes (+qaovh etc), simplemodes, parameter modes.
	 * This does not update the timestamp of the target channel, this must be done seperately.
	 */
	static void RemoveStatus(Channel* c);

	/**
	 * Lowers the TS on the given channel: removes all modes, unsets all extensions,
	 * clears the topic and removes all pending invites.
	 * @param chan The target channel whose TS to lower
	 * @param TS The new TS to set
	 * @param newname The new name of the channel; must be the same or a case change of the current name
	 */
	static void LowerTS(Channel* chan, time_t TS, const std::string& newname);
	void ProcessModeUUIDPair(const std::string& item, TreeServer* sourceserver, Channel* chan, Modes::ChangeList* modechangelist, FwdFJoinBuilder& fwdfjoin);
 public:
	CommandFJoin(Module* Creator) : ServerCommand(Creator, "FJOIN", 3) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_LOCALONLY; }

	class Builder : public CmdBuilder
	{
		/** Maximum possible Membership::Id length in decimal digits, used for determining whether a user will fit into
		 * a message or not
		 */
		static const size_t membid_max_digits = 20;
		static const size_t maxline = 510;
		std::string::size_type pos;

	protected:
		void add(Membership* memb, std::string::const_iterator mbegin, std::string::const_iterator mend);
		bool has_room(std::string::size_type nummodes) const;

	 public:
		Builder(Channel* chan, TreeServer* source = Utils->TreeRoot);
		void add(Membership* memb)
		{
			add(memb, memb->modes.begin(), memb->modes.end());
		}

		bool has_room(Membership* memb) const
		{
			return has_room(memb->modes.size());
		}

		void clear();
		const std::string& finalize();
	};
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
	CommandFTopic(Module* Creator) : ServerCommand(Creator, "FTOPIC", 4, 5) { }
	CmdResult Handle(User* user, std::vector<std::string>& params);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(Channel* chan);
		Builder(User* user, Channel* chan);
	};
};

class CommandFHost : public UserOnlyServerCommand<CommandFHost>
{
 public:
	CommandFHost(Module* Creator) : UserOnlyServerCommand<CommandFHost>(Creator, "FHOST", 1) { }
	CmdResult HandleRemote(RemoteUser* user, std::vector<std::string>& params);
};

class CommandFIdent : public UserOnlyServerCommand<CommandFIdent>
{
 public:
	CommandFIdent(Module* Creator) : UserOnlyServerCommand<CommandFIdent>(Creator, "FIDENT", 1) { }
	CmdResult HandleRemote(RemoteUser* user, std::vector<std::string>& params);
};

class CommandFName : public UserOnlyServerCommand<CommandFName>
{
 public:
	CommandFName(Module* Creator) : UserOnlyServerCommand<CommandFName>(Creator, "FNAME", 1) { }
	CmdResult HandleRemote(RemoteUser* user, std::vector<std::string>& params);
};

class CommandIJoin : public UserOnlyServerCommand<CommandIJoin>
{
 public:
	CommandIJoin(Module* Creator) : UserOnlyServerCommand<CommandIJoin>(Creator, "IJOIN", 2) { }
	CmdResult HandleRemote(RemoteUser* user, std::vector<std::string>& params);
};

class CommandResync : public ServerOnlyServerCommand<CommandResync>
{
 public:
	CommandResync(Module* Creator) : ServerOnlyServerCommand<CommandResync>(Creator, "RESYNC", 1) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_LOCALONLY; }
};

class SpanningTree::CommandAway : public UserOnlyServerCommand<SpanningTree::CommandAway>
{
 public:
	CommandAway(Module* Creator) : UserOnlyServerCommand<SpanningTree::CommandAway>(Creator, "AWAY", 0, 2) { }
	CmdResult HandleRemote(::RemoteUser* user, std::vector<std::string>& parameters);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(User* user);
		Builder(User* user, const std::string& msg);
	};
};

class XLine;
class CommandAddLine : public ServerCommand
{
 public:
	CommandAddLine(Module* Creator) : ServerCommand(Creator, "ADDLINE", 6, 6) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(XLine* xline, User* user = ServerInstance->FakeClient);
	};
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

class CommandIdle : public UserOnlyServerCommand<CommandIdle>
{
 public:
	CommandIdle(Module* Creator) : UserOnlyServerCommand<CommandIdle>(Creator, "IDLE", 1) { }
	CmdResult HandleRemote(RemoteUser* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class SpanningTree::CommandNick : public UserOnlyServerCommand<SpanningTree::CommandNick>
{
 public:
	CommandNick(Module* Creator) : UserOnlyServerCommand<SpanningTree::CommandNick>(Creator, "NICK", 2) { }
	CmdResult HandleRemote(::RemoteUser* user, std::vector<std::string>& parameters);
};

class SpanningTree::CommandPing : public ServerCommand
{
 public:
	CommandPing(Module* Creator) : ServerCommand(Creator, "PING", 1) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class SpanningTree::CommandPong : public ServerOnlyServerCommand<SpanningTree::CommandPong>
{
 public:
	CommandPong(Module* Creator) : ServerOnlyServerCommand<SpanningTree::CommandPong>(Creator, "PONG", 1) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_UNICAST(parameters[0]); }
};

class CommandSave : public ServerCommand
{
 public:
	/** Timestamp of the uuid nick of all users who collided and got their nick changed to uuid
	 */
	static const time_t SavedTimestamp = 100;

	CommandSave(Module* Creator) : ServerCommand(Creator, "SAVE", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class SpanningTree::CommandServer : public ServerOnlyServerCommand<SpanningTree::CommandServer>
{
	static void HandleExtra(TreeServer* newserver, const std::vector<std::string>& params);

 public:
	CommandServer(Module* Creator) : ServerOnlyServerCommand<SpanningTree::CommandServer>(Creator, "SERVER", 3) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);

	class Builder : public CmdBuilder
	{
		void push_property(const char* key, const std::string& val)
		{
			push(key).push_raw('=').push_raw(val);
		}
	 public:
		Builder(TreeServer* server);
	};
};

class CommandSQuit : public ServerOnlyServerCommand<CommandSQuit>
{
 public:
	CommandSQuit(Module* Creator) : ServerOnlyServerCommand<CommandSQuit>(Creator, "SQUIT", 2) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);
};

class CommandSNONotice : public ServerCommand
{
 public:
	CommandSNONotice(Module* Creator) : ServerCommand(Creator, "SNONOTICE", 2) { }
	CmdResult Handle(User* user, std::vector<std::string>& parameters);
};

class CommandEndBurst : public ServerOnlyServerCommand<CommandEndBurst>
{
 public:
	CommandEndBurst(Module* Creator) : ServerOnlyServerCommand<CommandEndBurst>(Creator, "ENDBURST") { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);
};

class CommandSInfo : public ServerOnlyServerCommand<CommandSInfo>
{
 public:
	CommandSInfo(Module* Creator) : ServerOnlyServerCommand<CommandSInfo>(Creator, "SINFO", 2) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(TreeServer* server, const char* type, const std::string& value);
	};
};

class CommandNum : public ServerOnlyServerCommand<CommandNum>
{
 public:
	CommandNum(Module* Creator) : ServerOnlyServerCommand<CommandNum>(Creator, "NUM", 3) { }
	CmdResult HandleServer(TreeServer* server, std::vector<std::string>& parameters);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);

	class Builder : public CmdBuilder
	{
	 public:
		Builder(SpanningTree::RemoteUser* target, const Numeric::Numeric& numeric);
	};
};

class SpanningTreeCommands
{
 public:
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
	SpanningTree::CommandAway away;
	CommandAddLine addline;
	CommandDelLine delline;
	CommandEncap encap;
	CommandIdle idle;
	SpanningTree::CommandNick nick;
	SpanningTree::CommandPing ping;
	SpanningTree::CommandPong pong;
	CommandSave save;
	SpanningTree::CommandServer server;
	CommandSQuit squit;
	CommandSNONotice snonotice;
	CommandEndBurst endburst;
	CommandSInfo sinfo;
	CommandNum num;
	SpanningTreeCommands(ModuleSpanningTree* module);
};
