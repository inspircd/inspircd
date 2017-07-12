/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

#include "inspircd.h"
#include "cachetimer.h"

class TreeServer;
class TreeSocket;
class Link;
class Autoconnect;
class ModuleSpanningTree;
class SpanningTreeUtilities;
class CmdBuilder;

extern SpanningTreeUtilities* Utils;

/** Associative container type, mapping server names/ids to TreeServers
 */
typedef TR1NS::unordered_map<std::string, TreeServer*, irc::insensitive, irc::StrHashComp> server_hash;

/** Contains helper functions and variables for this module,
 * and keeps them out of the global namespace
 */
class SpanningTreeUtilities : public classbase
{
	CacheRefreshTimer RefreshTimer;

 public:
 	typedef std::set<TreeSocket*> TreeSocketSet;
	typedef std::map<TreeSocket*, std::pair<std::string, int> > TimeoutList;

	/** Creator module
	 */
	ModuleSpanningTree* Creator;

	/** Flatten links and /MAP for non-opers
	 */
	bool FlatLinks;
	/** Hide U-Lined servers in /MAP and /LINKS
	 */
	bool HideULines;
	/** Announce TS changes to channels on merge
	 */
	bool AnnounceTSChange;

	/** Allow modules marked as VF_OPTCOMMON to be mismatched when linking
	 */
	bool AllowOptCommon;

	/** Make snomasks +CQ quiet during bursts and splits
	 */
	bool quiet_bursts;

	/* Number of seconds that a server can go without ping
	 * before opers are warned of high latency.
	 */
	int PingWarnTime;
	/** This variable represents the root of the server tree
	 */
	TreeServer *TreeRoot;
	/** IPs allowed to link to us
	 */
	std::vector<std::string> ValidIPs;
	/** Hash of currently connected servers by name
	 */
	server_hash serverlist;
	/** Hash of currently known server ids
	 */
	server_hash sidlist;
	/** List of all outgoing sockets and their timeouts
	 */
	TimeoutList timeoutlist;
	/** Holds the data from the <link> tags in the conf
	 */
	std::vector<reference<Link> > LinkBlocks;
	/** Holds the data from the <autoconnect> tags in the conf
	 */
	std::vector<reference<Autoconnect> > AutoconnectBlocks;

	/** Ping frequency of server to server links
	 */
	int PingFreq;

	/** Initialise utility class
	 */
	SpanningTreeUtilities(ModuleSpanningTree* Creator);

	/** Prepare for class destruction
	 */
	CullResult cull() CXX11_OVERRIDE;

	/** Destroy class and free listeners etc
	 */
	~SpanningTreeUtilities();

	void RouteCommand(TreeServer* origin, CommandBase* cmd, const parameterlist& parameters, User* user);

	/** Send a message from this server to one other local or remote
	 */
	void DoOneToOne(const CmdBuilder& params, Server* target);

	/** Send a message from this server to all but one other, local or remote
	 */
	void DoOneToAllButSender(const CmdBuilder& params, TreeServer* omit);

	/** Send a message from this server to all others
	 */
	void DoOneToMany(const CmdBuilder& params);

	/** Read the spanningtree module's tags from the config file
	 */
	void ReadConfiguration();

	/** Handle nick collision
	 */
	bool DoCollision(User* u, TreeServer* server, time_t remotets, const std::string& remoteident, const std::string& remoteip, const std::string& remoteuid, const char* collidecmd);

	/** Compile a list of servers which contain members of channel c
	 */
	void GetListOfServersForChannel(Channel* c, TreeSocketSet& list, char status, const CUList& exempt_list);

	/** Find a server by name or SID
	 */
	TreeServer* FindServer(const std::string &ServerName);

	/** Find server by SID
	 */
	TreeServer* FindServerID(const std::string &id);

	/** Find a server based on a target string.
	 * @param target Target string where a command should be routed to. May be a server name, a sid, a nickname or a uuid.
	 */
	TreeServer* FindRouteTarget(const std::string& target);

	/** Find a server by glob mask
	 */
	TreeServer* FindServerMask(const std::string &ServerName);

	/** Find a link tag from a server name
	 */
	Link* FindLink(const std::string& name);

	/** Refresh the IP cache used for allowing inbound connections
	 */
	void RefreshIPCache();

	/** Sends a PRIVMSG or a NOTICE to a channel obeying an exempt list and an optional prefix
	 */
	void SendChannelMessage(const std::string& prefix, Channel* target, const std::string& text, char status, const CUList& exempt_list, const char* message_type, TreeSocket* omit = NULL);
};

inline void SpanningTreeUtilities::DoOneToMany(const CmdBuilder& params)
{
	DoOneToAllButSender(params, NULL);
}
