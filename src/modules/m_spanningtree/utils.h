/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __ST__UTIL__
#define __ST__UTIL__

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
	ServerSocketListener(SpanningTreeUtilities *u, int port, const std::string& addr, const std::string& Hook)
		: ListenSocketBase(port, addr, "servers", Hook), Utils(u)
	{
	}

	virtual void OnAcceptReady(int nfd);
};

typedef std::map<TreeServer*,TreeServer*> TreeServerList;

/** Contains helper functions and variables for this module,
 * and keeps them out of the global namespace
 */
class SpanningTreeUtilities : public classbase
{
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
	bool cull();

	/** Destroy class and free listeners etc
	 */
	~SpanningTreeUtilities();

	void RouteCommand(TreeServer*, const std::string&, const parameterlist&, User*);

	/** Send a message from this server to one other local or remote
	 */
	bool DoOneToOne(const std::string &prefix, const std::string &command, parameterlist &params, std::string target);

	/** Send a message from this server to all but one other, local or remote
	 */
	bool DoOneToAllButSender(const std::string &prefix, const std::string &command, parameterlist &params, std::string omit);

	/** Send a message from this server to all but one other, local or remote
	 */
	bool DoOneToAllButSender(const char* prefix, const char* command, parameterlist &params, std::string omit);

	/** Send a message from this server to all others
	 */
	bool DoOneToMany(const std::string &prefix, const std::string &command, parameterlist &params);

	/** Send a message from this server to all others
	 */
	bool DoOneToMany(const char* prefix, const char* command, parameterlist &params);

	/** Send a message from this server to all others, without doing any processing on the command (e.g. send it as-is with colons and all)
	 */
	bool DoOneToAllButSenderRaw(const std::string &data, const std::string &omit, const std::string &prefix, const irc::string &command, parameterlist &params);

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

	/** Find a link tag from a server name
	 */
	Link* FindLink(const std::string& name);

	/** Refresh the IP cache used for allowing inbound connections
	 */
	void RefreshIPCache();
};

#endif
