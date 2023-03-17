/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2017, 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "pingtimer.h"

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
class TreeServer : public Server {
    TreeServer* Parent;         /* Parent entry */
    TreeServer* Route;          /* Route entry */
    std::vector<TreeServer*> Children;  /* List of child objects */
    std::string VersionString;      /* Version string or empty string */

    /** Full version string including patch version and other info
     */
    std::string fullversion;
    std::string rawversion;

    TreeSocket* Socket;         /* Socket used to communicate with this server */

    /** Counter counting how many servers are bursting in front of this server, including
     * this server. Set to parents' value on construction then it is increased if the
     * server itself starts bursting. Decreased when a server on the path to this server
     * finishes burst.
     */
    unsigned int behind_bursting;

    /** True if this server has been lost in a split and is awaiting destruction
     */
    bool isdead;

    /** Timer handling PINGing the server and killing it on timeout
     */
    PingTimer pingtimer;

    /** This method is used to add this TreeServer to the
     * hash maps. It is only called by the constructors.
     */
    void AddHashEntry();

    /** Used by SQuit logic to recursively remove servers
     */
    void SQuitInternal(unsigned int& num_lost_servers, bool error);

    /** Remove the reference to this server from the hash maps
     */
    void RemoveHash();

  public:
    typedef std::vector<TreeServer*> ChildServers;
    FakeUser* const ServerUser;     /* User representing this server */
    const time_t age;

    unsigned int
    UserCount;         /* How many users are on this server? [note: doesn't care about +i] */
    unsigned int OperCount;         /* How many opers are on this server? */

    /** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
     * represents our own server. Therefore, it has no route, no parent, and
     * no socket associated with it. Its version string is our own local version.
     */
    TreeServer();

    /** When we create a new server, we call this constructor to initialize it.
     * This constructor initializes the server's Route and Parent, and sets up
     * its ping counters so that it will be pinged one minute from now.
     */
    TreeServer(const std::string& Name, const std::string& Desc,
               const std::string& id, TreeServer* Above, TreeSocket* Sock, bool Hide);

    /** SQuit a server connected to this server, removing the given server and all servers behind it
     * @param server Server to squit, must be directly below this server
     * @param reason Reason for quitting the server, sent to opers and other servers
     * @param error Whether the server is being squit because of an error.
     */
    void SQuitChild(TreeServer* server, const std::string& reason,
                    bool error = false);

    /** SQuit this server, removing this server and all servers behind it
     * @param reason Reason for quitting the server, sent to opers and other servers
     * @param error Whether the server is being squit because of an error.
     */
    void SQuit(const std::string& reason, bool error = false) {
        GetParent()->SQuitChild(this, reason, error);
    }

    static unsigned int QuitUsers(const std::string& reason);

    /** Get route.
     * The 'route' is defined as the locally-
     * connected server which can be used to reach this server.
     */
    TreeServer* GetRoute() const {
        return Route;
    }

    /** Returns true if this server is the tree root (i.e.: us)
     */
    bool IsRoot() const {
        return (this->Parent == NULL);
    }

    /** Returns true if this server is locally connected
     */
    bool IsLocal() const {
        return (this->Route == this);
    }

    /** Returns true if the server is awaiting destruction
     * @return True if the server is waiting to be culled and deleted, false otherwise
     */
    bool IsDead() const {
        return isdead;
    }

    /** Get server version string
     */
    const std::string& GetVersion() const {
        return VersionString;
    }

    /** Get the full version string of this server
     * @return The full version string of this server, including patch version and other info
     */
    const std::string& GetFullVersion() const {
        return fullversion;
    }

    /** Get the raw version string of this server
     */
    const std::string& GetRawVersion() const {
        return rawversion;
    }

    /** Round trip time of last ping
     */
    unsigned long rtt;

    /** When we received BURST from this server, used to calculate total burst time at ENDBURST.
     */
    uint64_t StartBurst;

    /** True if this server is hidden
     */
    bool Hidden;

    /** Get the TreeSocket pointer for local servers.
     * For remote servers, this returns NULL.
     */
    TreeSocket* GetSocket() const {
        return Socket;
    }

    /** Get the parent server.
     * For the root node, this returns NULL.
     */
    TreeServer* GetParent() const {
        return Parent;
    }

    /** Set the server version string
     */
    void SetVersion(const std::string& verstr) {
        VersionString = verstr;
    }

    /** Set the full version string
     * @param verstr The version string to set
     */
    void SetFullVersion(const std::string& verstr) {
        fullversion = verstr;
    }

    /** Set the raw version string
     */
    void SetRawVersion(const std::string& verstr) {
        rawversion = verstr;
    }

    /** Sets the description of this server. Called when the description of a remote server changes
     * and we are notified about it.
     * @param descstr The description to set
     */
    void SetDesc(const std::string& descstr) {
        description = descstr;
    }

    /** Return all child servers
     */
    const ChildServers& GetChildren() const {
        return Children;
    }

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
    bool IsBursting() const {
        return (StartBurst != 0);
    }

    /** Check whether this server is behind a bursting server or is itself bursting.
     * This can tell whether a user is on a part of the network that is still bursting.
     * @return True if this server is bursting or is behind a server that is bursting, false if it isn't
     */
    bool IsBehindBursting() const {
        return (behind_bursting != 0);
    }

    /** Set the bursting state of the server
     * @param startms Time the server started bursting, if 0 or omitted, use current time
     */
    void BeginBurst(uint64_t startms = 0);

    /** Register a PONG from the server
     */
    void OnPong() {
        pingtimer.OnPong();
    }

    CullResult cull() CXX11_OVERRIDE;

    /** Destructor, deletes ServerUser unless IsRoot()
     */
    ~TreeServer();

    /** Returns the TreeServer the given user is connected to
     * @param user The user whose server to return
     * @return The TreeServer this user is connected to.
     */
    static TreeServer* Get(User* user) {
        return static_cast<TreeServer*>(user->server);
    }
};
