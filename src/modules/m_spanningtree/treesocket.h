/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

#include "inspircd.h"

#include "utils.h"

/*
 * The server list in InspIRCd is maintained as two structures
 * which hold the data in different ways. Most of the time, we
 * want to very quickly obtain three pieces of information:
 *
 * (1) The information on a server
 * (2) The information on the server we must send data through
 *     to actually REACH the server we're after
 * (3) Potentially, the child/parent objects of this server
 *
 * The InspIRCd spanning protocol provides easy access to these
 * by storing the data firstly in a recursive structure, where
 * each item references its parent item, and a dynamic list
 * of child items, and another structure which stores the items
 * hashed, linearly. This means that if we want to find a server
 * by name quickly, we can look it up in the hash, avoiding
 * any O(n) lookups. If however, during a split or sync, we want
 * to apply an operation to a server, and any of its child objects
 * we can resort to recursion to walk the tree structure.
 * Any socket can have one of five states at any one time.
 *
 * CONNECTING:  indicates an outbound socket which is
 *                          waiting to be writeable.
 * WAIT_AUTH_1: indicates the socket is outbound and
 *                          has successfully connected, but has not
 *                          yet sent and received SERVER strings.
 * WAIT_AUTH_2: indicates that the socket is inbound
 *                          but has not yet sent and received
 *                          SERVER strings.
 * CONNECTED:   represents a fully authorized, fully
 *                          connected server.
 * DYING:       represents a server that has had an error.
 */
enum ServerState { CONNECTING, WAIT_AUTH_1, WAIT_AUTH_2, CONNECTED, DYING };

struct CapabData {
    reference<Link> link;           /* Link block used for this connection */
    reference<Autoconnect>
    ac;      /* Autoconnect used to cause this connection, if any */
    std::string
    ModuleList;         /* Required module list of other server from CAPAB */
    std::string
    OptModuleList;      /* Optional module list of other server from CAPAB */
    std::string ChanModes;
    std::string UserModes;
    std::map<std::string,std::string> CapKeys;  /* CAPAB keys from other server */
    std::string ourchallenge;       /* Challenge sent for challenge/response */
    std::string theirchallenge;     /* Challenge recv for challenge/response */
    int capab_phase;            /* Have sent CAPAB already */
    bool auth_fingerprint;          /* Did we auth using SSL certificate fingerprint */
    bool auth_challenge;            /* Did we auth using challenge/response */
    irc::sockets::sockaddrs remotesa; /* The remote socket address. */

    // Data saved from incoming SERVER command, for later use when our credentials have been accepted by the other party
    std::string description;
    std::string sid;
    std::string name;
    bool hidden;
};

/** Every SERVER connection inbound or outbound is represented by an object of
 * type TreeSocket. During setup, the object can be found in Utils->timeoutlist;
 * after setup, MyRoot will have been created as a child of Utils->TreeRoot
 */
class TreeSocket : public BufferedSocket {
    struct BurstState;

    std::string linkID;         /* Description for this link */
    ServerState LinkState;          /* Link state */
    CapabData* capab;           /* Link setup data (held until burst is sent) */
    TreeServer* MyRoot;         /* The server we are talking to */
    unsigned int proto_version;         /* Remote protocol version */

    /** True if we've sent our burst.
     * This only changes the behavior of message translation for 1202 protocol servers and it can be
     * removed once 1202 support is dropped.
     */
    bool burstsent;

    /** Checks if the given servername and sid are both free
     */
    bool CheckDuplicate(const std::string& servername, const std::string& sid);

    /** Send all ListModeBase modes set on the channel
     */
    void SendListModes(Channel* chan);

    /** Send all known information about a channel */
    void SyncChannel(Channel* chan, BurstState& bs);

    /** Send all users and their oper state, away state and metadata */
    void SendUsers(BurstState& bs);

    /** Send all additional info about the given server to this server */
    void SendServerInfo(TreeServer* from);

    /** Find the User source of a command given a prefix and a command string.
     * This connection must be fully up when calling this function.
     * @param prefix Prefix string to find the source User object for. Can be a sid, a uuid or a server name.
     * @param command The command whose source to find. This is required because certain commands (like mode
     * changes and kills) must be processed even if their claimed source doesn't exist. If the given command is
     * such a command and the source does not exist, the function returns a valid FakeUser that can be used to
     * to process the command with.
     * @return The command source to use when processing the command or NULL if the source wasn't found.
     * Note that the direction of the returned source is not verified.
     */
    User* FindSource(const std::string& prefix, const std::string& command);

    /** Finish the authentication phase of this connection.
     * Change the state of the connection to CONNECTED, create a TreeServer object for the server on the
     * other end of the connection using the details provided in the parameters, and finally send a burst.
     * @param remotename Name of the remote server
     * @param remotesid SID of the remote server
     * @param remotedesc Description of the remote server
     * @param hidden True if the remote server is hidden according to the configuration
     */
    void FinishAuth(const std::string& remotename, const std::string& remotesid,
                    const std::string& remotedesc, bool hidden);

    /** Authenticate the remote server.
     * Validate the parameters and find the link block that matches the remote server. In case of an error,
     * an appropriate snotice is generated, an ERROR message is sent and the connection is closed.
     * Failing to find a matching link block counts as an error.
     * @param params Parameters they sent in the SERVER command
     * @return Link block for the remote server, or NULL if an error occurred
     */
    Link* AuthRemote(const CommandBase::Params& params);

    /** Write a line on this socket with a new line character appended, skipping all translation for old protocols
     * @param line Line to write without a new line character at the end
     */
    void WriteLineNoCompat(const std::string& line);

  public:
    const time_t age;

    /** Because most of the I/O gubbins are encapsulated within
     * BufferedSocket, we just call the superclass constructor for
     * most of the action, and append a few of our own values
     * to it.
     */
    TreeSocket(Link* link, Autoconnect* myac, const irc::sockets::sockaddrs& sa);

    /** When a listening socket gives us a new file descriptor,
     * we must associate it with a socket without creating a new
     * connection. This constructor is used for this purpose.
     */
    TreeSocket(int newfd, ListenSocket* via, irc::sockets::sockaddrs* client,
               irc::sockets::sockaddrs* server);

    /** Get link state
     */
    ServerState GetLinkState() const {
        return LinkState;
    }

    /** Get challenge set in our CAPAB for challenge/response
     */
    const std::string& GetOurChallenge();

    /** Get challenge set in our CAPAB for challenge/response
     */
    void SetOurChallenge(const std::string &c);

    /** Get challenge set in their CAPAB for challenge/response
     */
    const std::string& GetTheirChallenge();

    /** Get challenge set in their CAPAB for challenge/response
     */
    void SetTheirChallenge(const std::string &c);

    /** Compare two passwords based on authentication scheme
     */
    bool ComparePass(const Link& link, const std::string &theirs);

    /** Clean up information used only during server negotiation
     */
    void CleanNegotiationInfo();

    CullResult cull() CXX11_OVERRIDE;
    /** Destructor
     */
    ~TreeSocket();

    /** Construct a password, optionally hashed with the other side's
     * challenge string
     */
    std::string MakePass(const std::string &password, const std::string &challenge);

    /** When an outbound connection finishes connecting, we receive
     * this event, and must send our SERVER string to the other
     * side. If the other side is happy, as outlined in the server
     * to server docs on the inspircd.org site, the other side
     * will then send back its own server string.
     */
    void OnConnected() CXX11_OVERRIDE;

    /** Handle socket error event
     */
    void OnError(BufferedSocketError e) CXX11_OVERRIDE;

    /** Sends an error to the remote server, and displays it locally to show
     * that it was sent.
     */
    void SendError(const std::string &errormessage);

    /** Recursively send the server tree with distances as hops.
     * This is used during network burst to inform the other server
     * (and any of ITS servers too) of what servers we know about.
     */
    void SendServers(TreeServer* Current, TreeServer* s);

    /** Returns module list as a string, filtered by filter
     * @param filter a module version bitmask, such as VF_COMMON or VF_OPTCOMMON
     */
    std::string MyModules(int filter);

    /** Returns mode list as a string, filtered by type.
     * @param type The type of modes to return.
     */
    std::string BuildModeList(ModeType type);

    /** Send my capabilities to the remote side
     */
    void SendCapabilities(int phase);

    /* Isolate and return the elements that are different between two lists */
    void ListDifference(const std::string &one, const std::string &two, char sep,
                        std::string& mleft, std::string& mright);

    bool Capab(const CommandBase::Params& params);

    /** Send one or more FJOINs for a channel of users.
     * If the length of a single line is more than 480-NICKMAX
     * in length, it is split over multiple lines.
     */
    void SendFJoins(Channel* c);

    /** Send G-, Q-, Z- and E-lines */
    void SendXLines();

    /** Send all known information about a channel */
    void SyncChannel(Channel* chan);

    /** This function is called when we want to send a netburst to a local
     * server. There is a set order we must do this, because for example
     * users require their servers to exist, and channels require their
     * users to exist. You get the idea.
     */
    void DoBurst(TreeServer* s);

    /** This function is called when we receive data from a remote
     * server.
     */
    void OnDataReady() CXX11_OVERRIDE;

    /** Send one or more complete lines down the socket
     */
    void WriteLine(const std::string& line);

    /** Handle ERROR command */
    void Error(CommandBase::Params& params);

    /** (local) -> SERVER
     */
    bool Outbound_Reply_Server(CommandBase::Params& params);

    /** (local) <- SERVER
     */
    bool Inbound_Server(CommandBase::Params& params);

    /** Handle IRC line split
     */
    void Split(const std::string& line, std::string& tags, std::string& prefix,
               std::string& command, CommandBase::Params& params);

    /** Process complete line from buffer
     */
    void ProcessLine(std::string &line);

    /** Process message tags received from a remote server. */
    void ProcessTag(User* source, const std::string& tag,
                    ClientProtocol::TagMap& tags);

    /** Process a message for a fully connected server. */
    void ProcessConnectedLine(std::string& tags, std::string& prefix,
                              std::string& command, CommandBase::Params& params);

    /** Handle socket timeout from connect()
     */
    void OnTimeout() CXX11_OVERRIDE;
    /** Handle server quit on close
     */
    void Close() CXX11_OVERRIDE;

    /** Fixes messages coming from old servers so the new command handlers understand them
     */
    bool PreProcessOldProtocolMessage(User*& who, std::string& cmd,
                                      CommandBase::Params& params);
};
