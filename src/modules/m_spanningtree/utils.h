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
typedef nspace::hash_map<std::string, TreeServer*, nspace::hash<string>, irc::StrHashComp> server_hash;

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
	/** Socket bindings for listening sockets
	 */
	std::vector<TreeSocket*> Bindings;
	/** This variable represents the root of the server tree
	 */
	TreeServer *TreeRoot;
	/** IPs allowed to link to us
	 */
	std::vector<std::string> ValidIPs;
	/** Hash of currently connected servers by name
	 */
	server_hash serverlist;
	/** Holds the data from the <link> tags in the conf
	 */
	std::vector<Link> LinkBlocks;
	/** Holds a bitmask of queued xline types waiting to be applied.
	 * Will be a mask containing values APPLY_GLINES, APPLY_KLINES,
	 * APPLY_QLINES and APPLY_ZLINES.
	 */
	int lines_to_apply;

	/** List of module pointers which can provide I/O abstraction
	 */
	hookmodules hooks;

	/** List of module names which can provide I/O abstraction
	 */
	std::vector<std::string> hooknames;

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
};

#endif
