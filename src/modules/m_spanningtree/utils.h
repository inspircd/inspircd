/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __ST__UTIL__
#define __ST__UTIL__

#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* Foward declarations */
class TreeServer;
class TreeSocket;
class Link;
class ModuleSpanningTree;

/* This hash_map holds the hash equivalent of the server
 * tree, used for rapid linear lookups.
 */
#ifdef WINDOWS
typedef nspace::hash_map<std::string, TreeServer*, nspace::hash_compare<string, less<string> > > server_hash;
#else
typedef nspace::hash_map<std::string, TreeServer*, nspace::hash<string>, irc::StrHashComp> server_hash;
#endif

typedef std::map<TreeServer*,TreeServer*> TreeServerList;

/** A group of modules that implement InspSocketHook
 * that we can use to hook our server to server connections.
 */
typedef std::map<irc::string, Module*> hookmodules;

/** Contains helper functions and variables for this module,
 * and keeps them out of the global namespace
 */
class SpanningTreeUtilities
{
 private:
	/** Creator server
	 */
	InspIRCd* ServerInstance;
 public:
	/** Creator module
	 */
	ModuleSpanningTree* Creator;
	/** Remote servers that are currently bursting
	 */
	server_hash RemoteServersBursting;
	/** Flatten links and /MAP for non-opers
	 */
	bool FlatLinks;
	/** Hide U-Lined servers in /MAP and /LINKS
	 */
	bool HideULines;
	/** Announce TS changes to channels on merge
	 */
	bool AnnounceTSChange;
	/** Synchronize timestamps between servers
	 */
	bool EnableTimeSync;
	/** Make snomasks +CQ quiet during bursts and splits
	 */
	bool quiet_bursts;
	/** Socket bindings for listening sockets
	 */
	std::vector<TreeSocket*> Bindings;
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
	/** Hash of servers currently bursting but not initialized as connected
	 */
	std::map<irc::string,TreeSocket*> burstingserverlist;
	/** Holds the data from the <link> tags in the conf
	 */
	std::vector<Link> LinkBlocks;
	/** Holds a bitmask of queued xline types waiting to be applied.
	 * Will be a mask containing values APPLY_GLINES, APPLY_KLINES,
	 * APPLY_QLINES and APPLY_ZLINES.
	 */
	int lines_to_apply;

	/** If this is true, this server is the master sync server for time
	 * synching - e.g. it is the server with its clock correct. It will
	 * send out the correct time at intervals.
	 */
	bool MasterTime;

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
	void GetListOfServersForChannel(chanrec* c, TreeServerList &list, char status, const CUList &exempt_list);
	/** Find a server by name
	 */
	TreeServer* FindServer(const std::string &ServerName);
	/** Find a remote bursting server by name
	 */
	TreeServer* FindRemoteBurstServer(TreeServer* Server);
	/** Set a remote server to bursting or not bursting
	 */
	void SetRemoteBursting(TreeServer* Server, bool bursting);
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

	TreeSocket* FindBurstingServer(const std::string &ServerName);

	void AddBurstingServer(const std::string &ServerName, TreeSocket* s);

	void DelBurstingServer(TreeSocket* s);
};

#endif
