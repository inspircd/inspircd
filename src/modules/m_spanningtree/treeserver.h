/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __TREESERVER_H__
#define __TREESERVER_H__

#include "treesocket.h"

/** Each server in the tree is represented by one class of
 * type TreeServer. A locally connected TreeServer can
 * have a class of type TreeSocket associated with it, for
 * remote servers, the TreeSocket entry will be NULL.
 * Each server also maintains a pointer to its parent
 * (NULL if this server is ours, at the top of the tree)
 * and a pointer to its "Route" (see the comments in the
 * constructors below), and also a dynamic list of pointers
 * to its children which can be iterated recursively
 * if required. Creating or deleting objects of type
 i* TreeServer automatically maintains the hash_map of
 * TreeServer items, deleting and inserting them as they
 * are created and destroyed.
 */
class TreeServer : public classbase
{
 public:
	TreeServer* const Parent;		/* Parent entry (like in /map); NULL only for local server */
	TreeSocket* const Socket;		/* Socket that this server was introduced on */
	FakeUser* const ServerUser;		/* User representing this server (server name and SID) */
	SpanningTreeUtilities* const Utils;	/* Utility class */
	std::vector<TreeServer*> Children;	/* List of child servers */
	std::string ServerDesc;			/* Server's description */
	std::string VersionString;		/* Version string (empty if unset) */
	std::string ModuleList;
	unsigned int UserCount;
	const time_t age;
	time_t NextPing;			/* After this time, the server should be PINGed*/
	unsigned long LastPingMsec;		/* Last ping time, in milliseconds */
	unsigned long rtt;			/* Round trip time of last ping */
	unsigned long StartBurst;		/* Time (in milliseconds) when we recieved BURST from this server */
	bool LastPingWasGood;			/* True if the server responded to the last PING with a PONG */
	bool Warned;				/* True if we've warned opers about high latency on this server */
	bool bursting;				/* whether or not this server is bursting */
	bool Hidden;

	/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
	 * represents our own server. Therefore, it has no route, no parent, and
	 * no socket associated with it. Its version string is our own local version.
	 */
	TreeServer(SpanningTreeUtilities* Util);

	/** When we create a new server, we call this constructor to initialize it.
	 * This constructor initializes the server's Route and Parent, and sets up
	 * its ping counters so that it will be pinged one minute from now.
	 */
	TreeServer(SpanningTreeUtilities* Util, const std::string& Name, const std::string& Desc, const std::string &id,
		TreeServer* Above, TreeSocket* Sock, bool Hide);

	int QuitUsers(const std::string &reason);

	/** This method is used to add the structure to the
	 * hash_map for linear searches. It is only called
	 * by the constructors.
	 */
	void AddHashEntry();

	/** This method removes the reference to this object
	 * from the hash_map which is used for linear searches.
	 * It is only called by the default destructor.
	 */
	void DelHashEntry();

	/** Get server name
	 */
	const std::string& GetName() { return ServerUser->server; }

	/** Get server description (GECOS)
	 */
	const std::string& GetDesc() { return ServerDesc; }

	/** Get server version string
	 */
	const std::string& GetVersion() { return VersionString; }

	/** Set time we are next due to ping this server
	 */
	void SetNextPingTime(time_t t);

	/** Get the time we are next due to ping this server
	 */
	time_t NextPingTime();

	/** True if the server answered their last ping
	 */
	bool AnsweredLastPing();

	/** Set the server as responding to its last ping
	 */
	void SetPingFlag();

	/** Get the TreeSocket pointer for local servers.
	 * For remote servers, this returns NULL.
	 */
	TreeSocket* GetSocket() { return Socket; }

	/** Get the parent server.
	 * For the root node, this returns NULL.
	 */
	TreeServer* GetParent() { return Parent; }

	/** Return number of child servers
	 */
	unsigned int ChildCount();

	/** Return a child server indexed 0..n
	 */
	TreeServer* GetChild(unsigned int n);

	/** Add a child server
	 */
	void AddChild(TreeServer* Child);

	/** Delete a child server, return false if it didn't exist.
	 */
	bool DelChild(TreeServer* Child);

	/** Removes child nodes of this node, and of that node, etc etc.
	 * This is used during netsplits to automatically tidy up the
	 * server tree. It is slow, we don't use it for much else.
	 */
	bool Tidy();

	/** Get server ID
	 */
	const std::string& GetID() { return ServerUser->uuid; }

	/** Marks a server as having finished bursting and performs appropriate actions.
	 */
	void FinishBurst();
	/** Recursive call for child servers */
	void FinishBurstInternal();

	CullResult cull();
	/** Destructor
	 */
	~TreeServer();
};

#endif
