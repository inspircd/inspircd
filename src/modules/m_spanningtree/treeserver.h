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


#pragma once

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
class TreeServer : public Server
{
	TreeServer* Parent;			/* Parent entry */
	TreeServer* Route;			/* Route entry */
	std::vector<TreeServer*> Children;	/* List of child objects */
	std::string VersionString;		/* Version string or empty string */

	/** Full version string including patch version and other info
	 */
	std::string fullversion;

	TreeSocket* Socket;			/* Socket used to communicate with this server */
	time_t NextPing;			/* After this time, the server should be PINGed*/
	bool LastPingWasGood;			/* True if the server responded to the last PING with a PONG */
	std::string sid;			/* Server ID */

	/** Counter counting how many servers are bursting in front of this server, including
	 * this server. Set to parents' value on construction then it is increased if the
	 * server itself starts bursting. Decreased when a server on the path to this server
	 * finishes burst.
	 */
	unsigned int behind_bursting;

	/** True if this server has been lost in a split and is awaiting destruction
	 */
	bool isdead;

	/** This method is used to add this TreeServer to the
	 * hash maps. It is only called by the constructors.
	 */
	void AddHashEntry();

	/** Used by SQuit logic to recursively remove servers
	 */
	void SQuitInternal(unsigned int& num_lost_servers);

	/** Remove the reference to this server from the hash maps
	 */
	void RemoveHash();

 public:
	typedef std::vector<TreeServer*> ChildServers;
	FakeUser* const ServerUser;		/* User representing this server */
	const time_t age;

	bool Warned;				/* True if we've warned opers about high latency on this server */

	unsigned int UserCount;			/* How many users are on this server? [note: doesn't care about +i] */
	unsigned int OperCount;			/* How many opers are on this server? */

	/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
	 * represents our own server. Therefore, it has no route, no parent, and
	 * no socket associated with it. Its version string is our own local version.
	 */
	TreeServer();

	/** When we create a new server, we call this constructor to initialize it.
	 * This constructor initializes the server's Route and Parent, and sets up
	 * its ping counters so that it will be pinged one minute from now.
	 */
	TreeServer(const std::string& Name, const std::string& Desc, const std::string& id, TreeServer* Above, TreeSocket* Sock, bool Hide);

	/** SQuit a server connected to this server, removing the given server and all servers behind it
	 * @param server Server to squit, must be directly below this server
	 * @param reason Reason for quitting the server, sent to opers and other servers
	 */
	void SQuitChild(TreeServer* server, const std::string& reason);

	/** SQuit this server, removing this server and all servers behind it
	 * @param reason Reason for quitting the server, sent to opers and other servers
	 */
	void SQuit(const std::string& reason)
	{
		GetParent()->SQuitChild(this, reason);
	}

	static unsigned int QuitUsers(const std::string& reason);

	/** Get route.
	 * The 'route' is defined as the locally-
	 * connected server which can be used to reach this server.
	 */
	TreeServer* GetRoute();

	/** Returns true if this server is the tree root (i.e.: us)
	 */
	bool IsRoot() const { return (this->Parent == NULL); }

	/** Returns true if this server is locally connected
	 */
	bool IsLocal() const { return (this->Route == this); }

	/** Returns true if the server is awaiting destruction
	 * @return True if the server is waiting to be culled and deleted, false otherwise
	 */
	bool IsDead() const { return isdead; }

	/** Get server version string
	 */
	const std::string& GetVersion();

	/** Get the full version string of this server
	 * @return The full version string of this server, including patch version and other info
	 */
	const std::string& GetFullVersion() const { return fullversion; }

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

	/** Set the full version string
	 * @param verstr The version string to set
	 */
	void SetFullVersion(const std::string& verstr) { fullversion = verstr; }

	/** Sets the description of this server. Called when the description of a remote server changes
	 * and we are notified about it.
	 * @param descstr The description to set
	 */
	void SetDesc(const std::string& descstr) { description = descstr; }

	/** Return all child servers
	 */
	const ChildServers& GetChildren() const { return Children; }

	/** Add a child server
	 */
	void AddChild(TreeServer* Child);

	/** Delete a child server, return false if it didn't exist.
	 */
	bool DelChild(TreeServer* Child);

	/** Get server ID
	 */
	const std::string& GetID();

	/** Marks a server as having finished bursting and performs appropriate actions.
	 */
	void FinishBurst();
	/** Recursive call for child servers */
	void FinishBurstInternal();

	/** (Re)check the uline state of this server
	 */
	void CheckULine();

	/** Get the bursting state of this server
	 * @return True if this server is bursting, false if it isn't
	 */
	bool IsBursting() const { return (StartBurst != 0); }

	/** Check whether this server is behind a bursting server or is itself bursting.
	 * This can tell whether a user is on a part of the network that is still bursting.
	 * @return True if this server is bursting or is behind a server that is bursting, false if it isn't
	 */
	bool IsBehindBursting() const { return (behind_bursting != 0); }

	/** Set the bursting state of the server
	 * @param startms Time the server started bursting, if 0 or omitted, use current time
	 */
	void BeginBurst(unsigned long startms = 0);

	CullResult cull();

	/** Destructor, deletes ServerUser unless IsRoot()
	 */
	~TreeServer();

	/** Returns the TreeServer the given user is connected to
	 * @param user The user whose server to return
	 * @return The TreeServer this user is connected to.
	 */
	static TreeServer* Get(User* user)
	{
		return static_cast<TreeServer*>(user->server);
	}
};
