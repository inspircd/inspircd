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


#ifndef M_SPANNINGTREE_TREESERVER_H
#define M_SPANNINGTREE_TREESERVER_H

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
	TreeServer* Parent;			/* Parent entry */
	TreeServer* Route;			/* Route entry */
	std::vector<TreeServer*> Children;	/* List of child objects */
	irc::string ServerName;			/* Server's name */
	std::string ServerDesc;			/* Server's description */
	std::string VersionString;		/* Version string or empty string */
	unsigned int ServerUserCount;		/* How many users are on this server? [note: doesn't care about +i] */
	unsigned int ServerOperCount;		/* How many opers are on this server? */
	TreeSocket* Socket;			/* For directly connected servers this points at the socket object */
	time_t NextPing;			/* After this time, the server should be PINGed*/
	bool LastPingWasGood;			/* True if the server responded to the last PING with a PONG */
	SpanningTreeUtilities* Utils;		/* Utility class */
	std::string sid;			/* Server ID */

	/** Set server ID
	 * @param id Server ID
	 * @throws CoreException on duplicate ID
	 */
	void SetID(const std::string &id);

 public:
	FakeUser* const ServerUser;		/* User representing this server */
	time_t age;

	bool Warned;				/* True if we've warned opers about high latency on this server */
	bool bursting;				/* whether or not this server is bursting */

	/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
	 * represents our own server. Therefore, it has no route, no parent, and
	 * no socket associated with it. Its version string is our own local version.
	 */
	TreeServer(SpanningTreeUtilities* Util, std::string Name, std::string Desc, const std::string &id);

	/** When we create a new server, we call this constructor to initialize it.
	 * This constructor initializes the server's Route and Parent, and sets up
	 * its ping counters so that it will be pinged one minute from now.
	 */
	TreeServer(SpanningTreeUtilities* Util, std::string Name, std::string Desc, const std::string &id, TreeServer* Above, TreeSocket* Sock, bool Hide);

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

	/** Get route.
	 * The 'route' is defined as the locally-
	 * connected server which can be used to reach this server.
	 */
	TreeServer* GetRoute();

	/** Get server name
	 */
	std::string GetName();

	/** Get server description (GECOS)
	 */
	const std::string& GetDesc();

	/** Get server version string
	 */
	const std::string& GetVersion();

	/** Set time we are next due to ping this server
	 */
	void SetNextPingTime(time_t t);

	/** Get the time we are next due to ping this server
	 */
	time_t NextPingTime();

	/** Last ping time in milliseconds, used to calculate round trip time
	 */
	unsigned long LastPingMsec;

	/** Round trip time of last ping
	 */
	unsigned long rtt;

	/** When we recieved BURST from this server, used to calculate total burst time at ENDBURST.
	 */
	unsigned long StartBurst;

	/** True if this server is hidden
	 */
	bool Hidden;

	/** True if the server answered their last ping
	 */
	bool AnsweredLastPing();

	/** Set the server as responding to its last ping
	 */
	void SetPingFlag();

	/** Get the number of users on this server.
	 */
	unsigned int GetUserCount();

	/** Increment or decrement the user count by diff.
	 */
	void SetUserCount(int diff);

	/** Gets the numbers of opers on this server.
	 */
	unsigned int GetOperCount();

	/** Increment or decrement the oper count by diff.
	 */
	void SetOperCount(int diff);

	/** Get the TreeSocket pointer for local servers.
	 * For remote servers, this returns NULL.
	 */
	TreeSocket* GetSocket();

	/** Get the parent server.
	 * For the root node, this returns NULL.
	 */
	TreeServer* GetParent();

	/** Set the server version string
	 */
	void SetVersion(const std::string &Version);

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
	const std::string& GetID();

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
