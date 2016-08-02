/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


#ifndef M_SPANNINGTREE_TREESOCKET_H
#define M_SPANNINGTREE_TREESOCKET_H

#include "socket.h"
#include "inspircd.h"
#include "xline.h"

#include "utils.h"

/*
 * The server list in InspIRCd is maintained as two structures
 * which hold the data in different ways. Most of the time, we
 * want to very quicky obtain three pieces of information:
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
 * CONNECTING:	indicates an outbound socket which is
 *							waiting to be writeable.
 * WAIT_AUTH_1:	indicates the socket is outbound and
 * 							has successfully connected, but has not
 *							yet sent and received SERVER strings.
 * WAIT_AUTH_2:	indicates that the socket is inbound
 * 							but has not yet sent and received
 *							SERVER strings.
 * CONNECTED:   represents a fully authorized, fully
 *							connected server.
 * DYING:       represents a server that has had an error.
 */
enum ServerState { CONNECTING, WAIT_AUTH_1, WAIT_AUTH_2, CONNECTED, DYING };

struct CapabData
{
	reference<Link> link;			/* Link block used for this connection */
	reference<Autoconnect> ac;		/* Autoconnect used to cause this connection, if any */
	std::string ModuleList;			/* Required module list of other server from CAPAB */
	std::string OptModuleList;		/* Optional module list of other server from CAPAB */
	std::string ChanModes;
	std::string UserModes;
	std::map<std::string,std::string> CapKeys;	/* CAPAB keys from other server */
	std::string ourchallenge;		/* Challenge sent for challenge/response */
	std::string theirchallenge;		/* Challenge recv for challenge/response */
	int capab_phase;			/* Have sent CAPAB already */
	bool auth_fingerprint;			/* Did we auth using SSL fingerprint */
	bool auth_challenge;			/* Did we auth using challenge/response */

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
class TreeSocket : public BufferedSocket
{
	SpanningTreeUtilities* Utils;		/* Utility class */
	std::string linkID;			/* Description for this link */
	ServerState LinkState;			/* Link state */
	CapabData* capab;			/* Link setup data (held until burst is sent) */
	TreeServer* MyRoot;			/* The server we are talking to */
	int proto_version;			/* Remote protocol version */
	bool ConnectionFailureShown; /* Set to true if a connection failure message was shown */

	static const unsigned int FMODE_MAX_LENGTH = 350;

	/** Checks if the given servername and sid are both free
	 */
	bool CheckDuplicate(const std::string& servername, const std::string& sid);

 public:
	time_t age;

	/** Because most of the I/O gubbins are encapsulated within
	 * BufferedSocket, we just call the superclass constructor for
	 * most of the action, and append a few of our own values
	 * to it.
	 */
	TreeSocket(SpanningTreeUtilities* Util, Link* link, Autoconnect* myac, const std::string& ipaddr);

	/** When a listening socket gives us a new file descriptor,
	 * we must associate it with a socket without creating a new
	 * connection. This constructor is used for this purpose.
	 */
	TreeSocket(SpanningTreeUtilities* Util, int newfd, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);

	/** Get link state
	 */
	ServerState GetLinkState();

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

	CullResult cull();
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
	virtual void OnConnected();

	/** Handle socket error event
	 */
	virtual void OnError(BufferedSocketError e);

	/** Sends an error to the remote server, and displays it locally to show
	 * that it was sent.
	 */
	void SendError(const std::string &errormessage);

	/** Recursively send the server tree with distances as hops.
	 * This is used during network burst to inform the other server
	 * (and any of ITS servers too) of what servers we know about.
	 * If at any point any of these servers already exist on the other
	 * end, our connection may be terminated. The hopcounts given
	 * by this function are relative, this doesn't matter so long as
	 * they are all >1, as all the remote servers re-calculate them
	 * to be relative too, with themselves as hop 0.
	 */
	void SendServers(TreeServer* Current, TreeServer* s, int hops);

	/** Returns module list as a string, filtered by filter
	 * @param filter a module version bitmask, such as VF_COMMON or VF_OPTCOMMON
	 */
	std::string MyModules(int filter);

	/** Send my capabilities to the remote side
	 */
	void SendCapabilities(int phase);

	/** Add modules to VF_COMMON list for backwards compatability */
	void CompatAddModules(std::vector<std::string>& modlist);

	/* Isolate and return the elements that are different between two lists */
	void ListDifference(const std::string &one, const std::string &two, char sep,
		std::string& mleft, std::string& mright);

	bool Capab(const parameterlist &params);

	/** This function forces this server to quit, removing this server
	 * and any users on it (and servers and users below that, etc etc).
	 * It's very slow and pretty clunky, but luckily unless your network
	 * is having a REAL bad hair day, this function shouldnt be called
	 * too many times a month ;-)
	 */
	void SquitServer(std::string &from, TreeServer* Current, int& num_lost_servers, int& num_lost_users);

	/** This is a wrapper function for SquitServer above, which
	 * does some validation first and passes on the SQUIT to all
	 * other remaining servers.
	 */
	void Squit(TreeServer* Current, const std::string &reason);

	/* Used on nick collision ... XXX ugly function HACK */
	int DoCollision(User *u, time_t remotets, const std::string &remoteident, const std::string &remoteip, const std::string &remoteuid);

	/** Send one or more FJOINs for a channel of users.
	 * If the length of a single line is more than 480-NICKMAX
	 * in length, it is split over multiple lines.
	 */
	void SendFJoins(Channel* c);

	/** Send G, Q, Z and E lines */
	void SendXLines();

	/** Send channel modes and topics */
	void SendChannelModes();

	/** send all users and their oper state/modes */
	void SendUsers();

	/** This function is called when we want to send a netburst to a local
	 * server. There is a set order we must do this, because for example
	 * users require their servers to exist, and channels require their
	 * users to exist. You get the idea.
	 */
	void DoBurst(TreeServer* s);

	/** This function is called when we receive data from a remote
	 * server.
	 */
	void OnDataReady();

	/** Send one or more complete lines down the socket
	 */
	void WriteLine(std::string line);

	/** Handle ERROR command */
	void Error(parameterlist &params);

	/** Remote AWAY */
	bool Away(const std::string &prefix, parameterlist &params);

	/** SAVE to resolve nick collisions without killing */
	bool ForceNick(const std::string &prefix, parameterlist &params);

	/** ENCAP command
	 */
	void Encap(User* who, parameterlist &params);

	/** OPERQUIT command
	 */
	bool OperQuit(const std::string &prefix, parameterlist &params);

	/** PONG
	 */
	bool LocalPong(const std::string &prefix, parameterlist &params);

	/** VERSION
	 */
	bool ServerVersion(const std::string &prefix, parameterlist &params);

	/** ADDLINE
	 */
	bool AddLine(const std::string &prefix, parameterlist &params);

	/** DELLINE
	 */
	bool DelLine(const std::string &prefix, parameterlist &params);

	/** WHOIS
	 */
	bool Whois(const std::string &prefix, parameterlist &params);

	/** PUSH
	 */
	bool Push(const std::string &prefix, parameterlist &params);

	/** PING
	 */
	bool LocalPing(const std::string &prefix, parameterlist &params);

	/** <- (remote) <- SERVER
	 */
	bool RemoteServer(const std::string &prefix, parameterlist &params);

	/** (local) -> SERVER
	 */
	bool Outbound_Reply_Server(parameterlist &params);

	/** (local) <- SERVER
	 */
	bool Inbound_Server(parameterlist &params);

	/** Handle IRC line split
	 */
	void Split(const std::string &line, std::string& prefix, std::string& command, parameterlist &params);

	/** Process complete line from buffer
	 */
	void ProcessLine(std::string &line);

	void ProcessConnectedLine(std::string& prefix, std::string& command, parameterlist& params);

	/** Handle socket timeout from connect()
	 */
	virtual void OnTimeout();
	/** Handle server quit on close
	 */
	virtual void Close();

	/** Returns true if this server was introduced to the rest of the network
	 */
	bool Introduced();
};

#endif

