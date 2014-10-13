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


#ifndef M_SPANNINGTREE_UTILS_H
#define M_SPANNINGTREE_UTILS_H

#include "inspircd.h"

/* Foward declarations */
class TreeServer;
class TreeSocket;
class Link;
class Autoconnect;
class ModuleSpanningTree;
class SpanningTreeUtilities;

/* This hash_map holds the hash equivalent of the server
 * tree, used for rapid linear lookups.
 */
#ifdef HASHMAP_DEPRECATED
	typedef nspace::hash_map<std::string, TreeServer*, nspace::insensitive, irc::StrHashComp> server_hash;
#else
	typedef nspace::hash_map<std::string, TreeServer*, nspace::hash<std::string>, irc::StrHashComp> server_hash;
#endif

typedef std::map<TreeServer*,TreeServer*> TreeServerList;

/** Contains helper functions and variables for this module,
 * and keeps them out of the global namespace
 */
class SpanningTreeUtilities : public classbase
{
 public:
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

	/** True (default) if we are to use challenge-response HMAC
	 * to authenticate passwords.
	 *
	 * NOTE: This defaults to on, but should be turned off if
	 * you are linking to an older version of inspircd.
	 */
	bool ChallengeResponse;

	/** Ping frequency of server to server links
	 */
	int PingFreq;

	/** Initialise utility class
	 */
	SpanningTreeUtilities(ModuleSpanningTree* Creator);

	/** Prepare for class destruction
	 */
	CullResult cull();

	/** Destroy class and free listeners etc
	 */
	~SpanningTreeUtilities();

	void RouteCommand(TreeServer*, const std::string&, const parameterlist&, User*);

	/** Send a message from this server to one other local or remote
	 */
	bool DoOneToOne(const std::string &prefix, const std::string &command, const parameterlist &params, const std::string& target);

	/** Send a message from this server to all but one other, local or remote
	 */
	bool DoOneToAllButSender(const std::string &prefix, const std::string &command, const parameterlist &params, const std::string& omit);

	/** Send a message from this server to all others
	 */
	bool DoOneToMany(const std::string &prefix, const std::string &command, const parameterlist &params);

	/** Read the spanningtree module's tags from the config file
	 */
	void ReadConfiguration();

	/** Add a server to the server list for GetListOfServersForChannel
	 */
	void AddThisServer(TreeServer* server, TreeServerList &list);

	/** Compile a list of servers which contain members of channel c
	 */
	void GetListOfServersForChannel(Channel* c, TreeServerList &list, char status, const CUList &exempt_list);

	/** Find a server by name
	 */
	TreeServer* FindServer(const std::string &ServerName);

	/** Find server by SID
	 */
	TreeServer* FindServerID(const std::string &id);

	/** Find a route to a server by name
	 */
	TreeServer* BestRouteTo(const std::string &ServerName);

	/** Find a server by glob mask
	 */
	TreeServer* FindServerMask(const std::string &ServerName);

	/** Find a link tag from a server name
	 */
	Link* FindLink(const std::string& name);

	/** Refresh the IP cache used for allowing inbound connections
	 */
	void RefreshIPCache();

	/** Recreate serverlist and sidlist, this is needed because of m_nationalchars changing
	 * national_case_insensitive_map which is used by the hash function
	 */
	void Rehash();
};

#endif
