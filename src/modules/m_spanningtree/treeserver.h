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

#ifndef __TREESERVER_H__
#define __TREESERVER_H__

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
	InspIRCd* ServerInstance;		/* Creator */
	TreeServer* Parent;			/* Parent entry */
	TreeServer* Route;			/* Route entry */
	std::vector<TreeServer*> Children;	/* List of child objects */
	irc::string ServerName;			/* Server's name */
	std::string ServerDesc;			/* Server's description */
	std::string VersionString;		/* Version string or empty string */
	int UserCount;				/* Not used in this version */
	int OperCount;				/* Not used in this version */
	TreeSocket* Socket;			/* For directly connected servers this points at the socket object */
	time_t NextPing;			/* After this time, the server should be PINGed*/
	bool LastPingWasGood;			/* True if the server responded to the last PING with a PONG */
	SpanningTreeUtilities* Utils;		/* Utility class */

 public:

	bool Warned;				/* True if we've warned opers about high latency on this server */

	/** We don't use this constructor. Its a dummy, and won't cause any insertion
	 * of the TreeServer into the hash_map. See below for the two we DO use.
	 */
	TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance);

	/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
	 * represents our own server. Therefore, it has no route, no parent, and
	 * no socket associated with it. Its version string is our own local version.
	 */
	TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc);
	
	/** When we create a new server, we call this constructor to initialize it.
	 * This constructor initializes the server's Route and Parent, and sets up
	 * its ping counters so that it will be pinged one minute from now.
	 */
	TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc, TreeServer* Above, TreeSocket* Sock, bool Hide);

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
	std::string GetDesc();

	/** Get server version string
	 */
	std::string GetVersion();

	/** Set time we are next due to ping this server
	 */
	void SetNextPingTime(time_t t);

	/** Get the time we are next due to ping this server
	 */
	time_t NextPingTime();

	/** Time of last ping used to calculate this->rtt below
	 */
	time_t LastPing;

	/** Last ping time in microseconds, used to calculate round trip time
	 */
	unsigned long LastPingMsec;

	/** Round trip time of last ping
	 */
	unsigned long rtt;

	/** True if this server is hidden
	 */
	bool Hidden;

	/** True if the server answered their last ping
	 */
	bool AnsweredLastPing();

	/** Set the server as responding to its last ping
	 */
	void SetPingFlag();

	/** Get the number of users on this server for MAP
	 */
	int GetUserCount();

	/** Increment the user counter
	 */
	void AddUserCount();

	/** Decrement the user counter
	 */
	void DelUserCount();

	/** Get the oper count for this server
	 */
	int GetOperCount();

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

	/** Destructor
	 */
	~TreeServer();

};

#endif
