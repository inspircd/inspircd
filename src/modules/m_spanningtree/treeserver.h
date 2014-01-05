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
	TreeSocket* Socket;			/* Socket used to communicate with this server */
	time_t NextPing;			/* After this time, the server should be PINGed*/
	bool LastPingWasGood;			/* True if the server responded to the last PING with a PONG */
	std::string sid;			/* Server ID */

	/** This method is used to add this TreeServer to the
	 * hash maps. It is only called by the constructors.
	 */
	void AddHashEntry();

 public:
	typedef std::vector<TreeServer*> ChildServers;
	FakeUser* const ServerUser;		/* User representing this server */
	const time_t age;

	bool Warned;				/* True if we've warned opers about high latency on this server */
	bool bursting;				/* whether or not this server is bursting */

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

	int QuitUsers(const std::string &reason);

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

	/** Return all child servers
	 */
	const ChildServers& GetChildren() const { return Children; }

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
	void Tidy();

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

	CullResult cull();

	/** Destructor
	 * Removes the reference to this object from the
	 * hash maps.
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
