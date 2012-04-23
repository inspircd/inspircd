/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#ifndef __ST__UTIL__
#define __ST__UTIL__

#include "inspircd.h"

/* Foward declarations */
class TreeServer;
class TreeSocket;
class Link;
class ModuleSpanningTree;
class SpanningTreeUtilities;

/* This hash_map holds the hash equivalent of the server
 * tree, used for rapid linear lookups.
 */
#if defined(WINDOWS) && !defined(HASHMAP_DEPRECATED)
	typedef nspace::hash_map<std::string, TreeServer*, nspace::hash_compare<std::string, std::less<std::string> > > server_hash;
#else
	#ifdef HASHCOMP_DEPRECATED
		typedef nspace::hash_map<std::string, TreeServer*, nspace::insensitive, irc::StrHashComp> server_hash;
	#else
		typedef nspace::hash_map<std::string, TreeServer*, nspace::hash<std::string>, irc::StrHashComp> server_hash;
	#endif
#endif

/*
 * Initialises server connections
 */
class ServerSocketListener : public ListenSocketBase
{
	SpanningTreeUtilities *Utils;

 public:
	ServerSocketListener(InspIRCd* Instance, SpanningTreeUtilities *u, int port, char* addr) : ListenSocketBase(Instance, port, addr)
	{
		this->Utils = u;
	}

	virtual void OnAcceptReady(const std::string &ipconnectedto, int nfd, const std::string &incomingip);
};

typedef std::map<TreeServer*,TreeServer*> TreeServerList;

/** A group of modules that implement BufferedSocketHook
 * that we can use to hook our server to server connections.
 */
typedef std::map<irc::string, Module*> hookmodules;

/** Contains helper functions and variables for this module,
 * and keeps them out of the global namespace
 */
class SpanningTreeUtilities : public classbase
{
 private:
	/** Creator server
	 */
	InspIRCd* ServerInstance;
 public:
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

	/** Make snomasks +CQ quiet during bursts and splits
	 */
	bool quiet_bursts;

	/** Socket bindings for listening sockets
	 */
	std::vector<ServerSocketListener *> Bindings;
	/* Number of seconds that a server can go without ping
	 * before opers are warned of high latency.
	 */
	int PingWarnTime;
	/** This variable represents the root of the server tree
	 */
	TreeServer *TreeRoot;
	/** Represents the server whose command we are processing
	 */
	FakeUser *ServerUser;
	/** IPs allowed to link to us
	 */
	std::vector<std::string> ValidIPs;
	/** Hash of currently connected servers by name
	 */
	server_hash serverlist;
	/** Hash of currently known server ids
	 */
	server_hash sidlist;
	/** Hash of servers currently bursting but not initialized as connected
	 */
	std::map<irc::string,TreeSocket*> burstingserverlist;
	/** List of all outgoing sockets and their timeouts
	 */
	std::map<TreeSocket*, std::pair<std::string, int> > timeoutlist;
	/** Holds the data from the <link> tags in the conf
	 */
	std::vector<Link> LinkBlocks;

	/** List of module pointers which can provide I/O abstraction
	 */
	hookmodules hooks;

	/** List of module names which can provide I/O abstraction
	 */
	std::vector<std::string> hooknames;

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
	SpanningTreeUtilities(InspIRCd* Instance, ModuleSpanningTree* Creator);

	/** Destroy class and free listeners etc
	 */
	~SpanningTreeUtilities();

	/** Send a message from this server to one other local or remote
	 */
	bool DoOneToOne(const std::string &prefix, const std::string &command, std::deque<std::string> &params, std::string target);

	/** Send a message from this server to all but one other, local or remote
	 */
	bool DoOneToAllButSender(const std::string &prefix, const std::string &command, std::deque<std::string> &params, std::string omit);

	/** Send a message from this server to all but one other, local or remote
	 */
	bool DoOneToAllButSender(const char* prefix, const char* command, std::deque<std::string> &params, std::string omit);

	/** Send a message from this server to all others
	 */
	bool DoOneToMany(const std::string &prefix, const std::string &command, std::deque<std::string> &params);

	/** Send a message from this server to all others
	 */
	bool DoOneToMany(const char* prefix, const char* command, std::deque<std::string> &params);

	/** Send a message from this server to all others, without doing any processing on the command (e.g. send it as-is with colons and all)
	 */
	bool DoOneToAllButSenderRaw(const std::string &data, const std::string &omit, const std::string &prefix, const irc::string &command, std::deque<std::string> &params);

	/** Read the spanningtree module's tags from the config file
	 */
	void ReadConfiguration(bool rebind);

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

	/** Returns true if this is a server name we recognise
	 */
	bool IsServer(const std::string &ServerName);

	/** Attempt to connect to the failover link of link x
	 */
	void DoFailOver(Link* x);

	/** Find a link tag from a server name
	 */
	Link* FindLink(const std::string& name);

	/** Refresh the IP cache used for allowing inbound connections
	 */
	void RefreshIPCache();
};

#endif
