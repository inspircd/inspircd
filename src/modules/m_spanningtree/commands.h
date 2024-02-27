/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
#include "modules/away.h"

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

class CommandRConnect final
	: public Command
{
public:
	CommandRConnect(Module* Creator);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class CommandRSQuit final
	: public Command
{
public:
	CommandRSQuit(Module* Creator);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class CommandMap final
	: public Command
{
public:
	CommandMap(Module* Creator);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class CommandMetadata final
	: public ServerCommand
{
public:
	CommandMetadata(Module* Creator)
		: ServerCommand(Creator, "METADATA", 2)
	{
	}
	CmdResult Handle(User* user, Params& params) override;

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(const Extensible* ext, const std::string& key, const std::string& val);
		Builder(const std::string& key, const std::string& val);
	};
};

class CommandUID final
	: public ServerOnlyServerCommand<CommandUID>
{
public:
	CommandUID(Module* Creator)
		: ServerOnlyServerCommand<CommandUID>(Creator, "UID", 10)
	{
	}
	CmdResult HandleServer(TreeServer* server, CommandBase::Params& params);

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(User* user, bool real_user);
	};
};

class CommandOpertype final
	: public UserOnlyServerCommand<CommandOpertype>
{
public:
	CommandOpertype(Module* Creator)
		: UserOnlyServerCommand<CommandOpertype>(Creator, "OPERTYPE", 1)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& params);

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(User* user, const std::shared_ptr<OperAccount>& oper, bool automatic = false);
	};
};

class TreeSocket;
class FwdFJoinBuilder;
class CommandFJoin final
	: public ServerCommand
{
	/** Remove all modes from a channel, including statusmodes (+qaovh etc), simplemodes, parameter modes.
	 * This does not update the timestamp of the target channel, this must be done separately.
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
	static void ProcessModeUUIDPair(const std::string& item, TreeServer* sourceserver, Channel* chan, Modes::ChangeList* modechangelist, FwdFJoinBuilder& fwdfjoin);
public:
	CommandFJoin(Module* Creator)
		: ServerCommand(Creator, "FJOIN", 3)
	{
	}
	CmdResult Handle(User* user, Params& params) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override { return ROUTE_LOCALONLY; }

	class Builder
		: public CmdBuilder
	{
		/** Maximum possible Membership::Id length in decimal digits, used for determining whether a user will fit into
		 * a message or not
		 */
		static constexpr size_t membid_max_digits = 20;
		static constexpr size_t maxline = 510;
		std::string::size_type pos;

	protected:
		void add(Membership* memb, std::string::const_iterator mbegin, std::string::const_iterator mend);

	public:
		Builder(Channel* chan, TreeServer* source = Utils->TreeRoot);

		void add(Membership* memb)
		{
			const std::string modes = memb->GetAllPrefixModes();
			add(memb, modes.begin(), modes.end());
		}

		void clear();

		const std::string& finalize();
	};
};

class CommandFMode final
	: public ServerCommand
{
public:
	CommandFMode(Module* Creator)
		: ServerCommand(Creator, "FMODE", 3)
	{
	}
	CmdResult Handle(User* user, Params& params) override;
};

class CommandFTopic final
	: public ServerCommand
{
public:
	CommandFTopic(Module* Creator)
		: ServerCommand(Creator, "FTOPIC", 4, 5)
	{
	}
	CmdResult Handle(User* user, Params& params) override;

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(Channel* chan);
		Builder(User* user, Channel* chan);
	};
};

class CommandFHost final
	: public UserOnlyServerCommand<CommandFHost>
{
public:
	CommandFHost(Module* Creator)
		: UserOnlyServerCommand<CommandFHost>(Creator, "FHOST", 1)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& params);
};

class CommandFRHost final
	: public UserOnlyServerCommand<CommandFRHost>
{
public:
	CommandFRHost(Module* Creator)
		: UserOnlyServerCommand<CommandFRHost>(Creator, "FRHOST", 1)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& params);
};

class CommandFIdent final
	: public UserOnlyServerCommand<CommandFIdent>
{
public:
	CommandFIdent(Module* Creator)
		: UserOnlyServerCommand<CommandFIdent>(Creator, "FIDENT", 2)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& params);
};

class CommandFName final
	: public UserOnlyServerCommand<CommandFName>
{
public:
	CommandFName(Module* Creator)
		: UserOnlyServerCommand<CommandFName>(Creator, "FNAME", 1)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& params);
};

class CommandIJoin final
	: public UserOnlyServerCommand<CommandIJoin>
{
public:
	CommandIJoin(Module* Creator)
		: UserOnlyServerCommand<CommandIJoin>(Creator, "IJOIN", 2)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& params);
};

class CommandResync final
	: public ServerOnlyServerCommand<CommandResync>
{
public:
	CommandResync(Module* Creator)
		: ServerOnlyServerCommand<CommandResync>(Creator, "RESYNC", 1)
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);
	RouteDescriptor GetRouting(User* user, const Params& parameters) override { return ROUTE_LOCALONLY; }
};

class SpanningTree::CommandAway final
	: public UserOnlyServerCommand<SpanningTree::CommandAway>
{
private:
	Away::EventProvider awayevprov;

public:
	CommandAway(Module* Creator)
		: UserOnlyServerCommand<SpanningTree::CommandAway>(Creator, "AWAY", 0, 2)
		, awayevprov(Creator)
	{
	}
	CmdResult HandleRemote(::RemoteUser* user, Params& parameters);

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(User* user);
	};
};

class XLine;
class CommandAddLine final
	: public ServerCommand
{
public:
	CommandAddLine(Module* Creator)
		: ServerCommand(Creator, "ADDLINE", 6, 6)
	{
	}
	CmdResult Handle(User* user, Params& parameters) override;

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(XLine* xline, User* user = ServerInstance->FakeClient);
	};
};

class CommandDelLine final
	: public ServerCommand
{
public:
	CommandDelLine(Module* Creator)
		: ServerCommand(Creator, "DELLINE", 2, 2)
	{
	}
	CmdResult Handle(User* user, Params& parameters) override;
};

class CommandEncap final
	: public ServerCommand
{
public:
	CommandEncap(Module* Creator)
		: ServerCommand(Creator, "ENCAP", 2)
	{
	}
	CmdResult Handle(User* user, Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class CommandIdle final
	: public UserOnlyServerCommand<CommandIdle>
{
public:
	CommandIdle(Module* Creator)
		: UserOnlyServerCommand<CommandIdle>(Creator, "IDLE", 1)
	{
	}
	CmdResult HandleRemote(RemoteUser* user, Params& parameters);
	RouteDescriptor GetRouting(User* user, const Params& parameters) override { return ROUTE_UNICAST(parameters[0]); }
};

class SpanningTree::CommandNick final
	: public UserOnlyServerCommand<SpanningTree::CommandNick>
{
public:
	CommandNick(Module* Creator)
		: UserOnlyServerCommand<SpanningTree::CommandNick>(Creator, "NICK", 2)
	{
	}
	CmdResult HandleRemote(::RemoteUser* user, Params& parameters);
};

class SpanningTree::CommandPing final
	: public ServerCommand
{
public:
	CommandPing(Module* Creator)
		: ServerCommand(Creator, "PING", 1)
	{
	}
	CmdResult Handle(User* user, Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override { return ROUTE_UNICAST(parameters[0]); }
};

class SpanningTree::CommandPong final
	: public ServerOnlyServerCommand<SpanningTree::CommandPong>
{
public:
	CommandPong(Module* Creator)
		: ServerOnlyServerCommand<SpanningTree::CommandPong>(Creator, "PONG", 1)
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);
	RouteDescriptor GetRouting(User* user, const Params& parameters) override { return ROUTE_UNICAST(parameters[0]); }
};

class DllExport CommandSave final
	: public ServerCommand
{
public:
	/** Timestamp of the uuid nick of all users who collided and got their nick changed to uuid
	 */
	static constexpr time_t SavedTimestamp = 100;

	CommandSave(Module* Creator)
		: ServerCommand(Creator, "SAVE", 2)
	{
	}
	CmdResult Handle(User* user, Params& parameters) override;
};

class SpanningTree::CommandServer final
	: public ServerOnlyServerCommand<SpanningTree::CommandServer>
{
	static void HandleExtra(TreeServer* newserver, Params& params);

public:
	CommandServer(Module* Creator)
		: ServerOnlyServerCommand<SpanningTree::CommandServer>(Creator, "SERVER", 3)
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);

	class Builder final
		: public CmdBuilder
	{
		void push_property(const char* key, const std::string& val)
		{
			push(key).push_raw('=').push_raw(val);
		}
	public:
		Builder(TreeServer* server);
	};
};

class CommandSQuit final
	: public ServerOnlyServerCommand<CommandSQuit>
{
public:
	CommandSQuit(Module* Creator)
		: ServerOnlyServerCommand<CommandSQuit>(Creator, "SQUIT", 2)
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);
};

class CommandSNONotice final
	: public ServerCommand
{
public:
	CommandSNONotice(Module* Creator)
		: ServerCommand(Creator, "SNONOTICE", 2)
	{
	}
	CmdResult Handle(User* user, Params& parameters) override;
};

class CommandEndBurst final
	: public ServerOnlyServerCommand<CommandEndBurst>
{
public:
	CommandEndBurst(Module* Creator)
		: ServerOnlyServerCommand<CommandEndBurst>(Creator, "ENDBURST")
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);
};

class CommandSInfo final
	: public ServerOnlyServerCommand<CommandSInfo>
{
public:
	CommandSInfo(Module* Creator)
		: ServerOnlyServerCommand<CommandSInfo>(Creator, "SINFO", 2)
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(TreeServer* server, const char* type, const std::string& value);
	};
};

class CommandNum final
	: public ServerOnlyServerCommand<CommandNum>
{
public:
	CommandNum(Module* Creator)
		: ServerOnlyServerCommand<CommandNum>(Creator, "NUM", 3)
	{
	}
	CmdResult HandleServer(TreeServer* server, Params& parameters);
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;

	class Builder final
		: public CmdBuilder
	{
	public:
		Builder(SpanningTree::RemoteUser* target, const Numeric::Numeric& numeric);
	};
};

class CommandLMode final
	: public ServerCommand
{
public:
	CommandLMode(Module* Creator)
		: ServerCommand(Creator, "LMODE", 3)
	{
	}
	CmdResult Handle(User* user, Params& params) override;
};

class SpanningTreeCommands final
{
public:
	CommandMetadata metadata;
	CommandUID uid;
	CommandOpertype opertype;
	CommandFJoin fjoin;
	CommandIJoin ijoin;
	CommandResync resync;
	CommandFMode fmode;
	CommandFTopic ftopic;
	CommandFHost fhost;
	CommandFHost frhost;
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
	CommandLMode lmode;
	SpanningTreeCommands(ModuleSpanningTree* module);
};
