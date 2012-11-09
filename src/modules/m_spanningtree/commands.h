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


#ifndef M_SPANNINGTREE_COMMANDS_H
#define M_SPANNINGTREE_COMMANDS_H

#include "main.h"

/** Handle /RCONNECT
 */
class CommandRConnect : public Command
{
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRConnect (Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class CommandRSQuit : public Command
{
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRSQuit(Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
        void NoticeUser(User* user, const std::string &msg);
};

class CommandSVSJoin : public Command
{
 public:
	CommandSVSJoin(Module* Creator) : Command(Creator, "SVSJOIN", 2) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};
class CommandSVSPart : public Command
{
 public:
	CommandSVSPart(Module* Creator) : Command(Creator, "SVSPART", 2) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};
class CommandSVSNick : public Command
{
 public:
	CommandSVSNick(Module* Creator) : Command(Creator, "SVSNICK", 3) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};
class CommandMetadata : public Command
{
 public:
	CommandMetadata(Module* Creator) : Command(Creator, "METADATA", 2) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandUID : public Command
{
 public:
	CommandUID(Module* Creator) : Command(Creator, "UID", 10) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandOpertype : public Command
{
 public:
	CommandOpertype(Module* Creator) : Command(Creator, "OPERTYPE", 1) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandFJoin : public Command
{
 public:
	CommandFJoin(Module* Creator) : Command(Creator, "FJOIN", 3) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
	/** Remove all modes from a channel, including statusmodes (+qaovh etc), simplemodes, parameter modes.
	 * This does not update the timestamp of the target channel, this must be done seperately.
	 */
	void RemoveStatus(User* source, parameterlist &params);
};
class CommandFMode : public Command
{
 public:
	CommandFMode(Module* Creator) : Command(Creator, "FMODE", 3) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandFTopic : public Command
{
 public:
	CommandFTopic(Module* Creator) : Command(Creator, "FTOPIC", 4) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandFHost : public Command
{
 public:
	CommandFHost(Module* Creator) : Command(Creator, "FHOST", 1) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandFIdent : public Command
{
 public:
	CommandFIdent(Module* Creator) : Command(Creator, "FIDENT", 1) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
};
class CommandFName : public Command
{
 public:
	CommandFName(Module* Creator) : Command(Creator, "FNAME", 1) { flags_needed = FLAG_SERVERONLY; }
	CmdResult Handle (const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters) { return ROUTE_BROADCAST; }
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
	CommandFMode fmode;
	CommandFTopic ftopic;
	CommandFHost fhost;
	CommandFIdent fident;
	CommandFName fname;
	SpanningTreeCommands(ModuleSpanningTree* module);
};

#endif
