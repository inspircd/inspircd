#ifndef __TREESOCKET_H__
#define __TREESOCKET_H__

#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/utils.h"

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
 * The LISTENER state indicates a socket which is listening
 * for connections. It cannot receive data itself, only incoming
 * sockets.
 * The CONNECTING state indicates an outbound socket which is
 * waiting to be writeable.
 * The WAIT_AUTH_1 state indicates the socket is outbound and
 * has successfully connected, but has not yet sent and received
 * SERVER strings.
 * The WAIT_AUTH_2 state indicates that the socket is inbound
 * (allocated by a LISTENER) but has not yet sent and received
 * SERVER strings.
 * The CONNECTED state represents a fully authorized, fully
 * connected server.
 */
enum ServerState { LISTENER, CONNECTING, WAIT_AUTH_1, WAIT_AUTH_2, CONNECTED };

/** Every SERVER connection inbound or outbound is represented by
 * an object of type TreeSocket.
 * TreeSockets, being inherited from InspSocket, can be tied into
 * the core socket engine, and we cn therefore receive activity events
 * for them, just like activex objects on speed. (yes really, that
 * is a technical term!) Each of these which relates to a locally
 * connected server is assocated with it, by hooking it onto a
 * TreeSocket class using its constructor. In this way, we can
 * maintain a list of servers, some of which are directly connected,
 * some of which are not.
 */
class TreeSocket : public InspSocket
{
	SpanningTreeUtilities* Utils;
	std::string myhost;
	std::string in_buffer;
	ServerState LinkState;
	std::string InboundServerName;
	std::string InboundDescription;
	int num_lost_users;
	int num_lost_servers;
	time_t NextPing;
	bool LastPingWasGood;
	bool bursting;
	unsigned int keylength;
	std::string ModuleList;
	std::map<std::string,std::string> CapKeys;
	Module* Hook;

 public:

	/** Because most of the I/O gubbins are encapsulated within
	 * InspSocket, we just call the superclass constructor for
	 * most of the action, and append a few of our own values
	 * to it.
	 */
	TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, Module* HookMod = NULL);

	TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, std::string ServerName, Module* HookMod = NULL);

	/** When a listening socket gives us a new file descriptor,
	 * we must associate it with a socket without creating a new
	 * connection. This constructor is used for this purpose.
	 */
	TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, int newfd, char* ip, Module* HookMod = NULL);

	ServerState GetLinkState();

	Module* GetHook();

	~TreeSocket();

	/** When an outbound connection finishes connecting, we receive
	 * this event, and must send our SERVER string to the other
	 * side. If the other side is happy, as outlined in the server
	 * to server docs on the inspircd.org site, the other side
	 * will then send back its own server string.
	 */
	virtual bool OnConnected();

	virtual void OnError(InspSocketError e);

	virtual int OnDisconnect();

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

	std::string MyCapabilities();

	void SendCapabilities();

	/* Check a comma seperated list for an item */
	bool HasItem(const std::string &list, const std::string &item);

	/* Isolate and return the elements that are different between two comma seperated lists */
	std::string ListDifference(const std::string &one, const std::string &two);

	bool Capab(const std::deque<std::string> &params);

	/** This function forces this server to quit, removing this server
	 * and any users on it (and servers and users below that, etc etc).
	 * It's very slow and pretty clunky, but luckily unless your network
	 * is having a REAL bad hair day, this function shouldnt be called
	 * too many times a month ;-)
	 */
	void SquitServer(std::string &from, TreeServer* Current);

	/** This is a wrapper function for SquitServer above, which
	 * does some validation first and passes on the SQUIT to all
	 * other remaining servers.
	 */
	void Squit(TreeServer* Current, const std::string &reason);

	/** FMODE command - server mode with timestamp checks */
	bool ForceMode(const std::string &source, std::deque<std::string> &params);

	/** FTOPIC command */
	bool ForceTopic(const std::string &source, std::deque<std::string> &params);

	/** FJOIN, similar to TS6 SJOIN, but not quite. */
	bool ForceJoin(const std::string &source, std::deque<std::string> &params);

	/** NICK command */
	bool IntroduceClient(const std::string &source, std::deque<std::string> &params);

	/** Send one or more FJOINs for a channel of users.
	 * If the length of a single line is more than 480-NICKMAX
	 * in length, it is split over multiple lines.
	 */
	void SendFJoins(TreeServer* Current, chanrec* c);

	/** Send G, Q, Z and E lines */
	void SendXLines(TreeServer* Current);

	/** Send channel modes and topics */
	void SendChannelModes(TreeServer* Current);

	/** send all users and their oper state/modes */
	void SendUsers(TreeServer* Current);

	/** This function is called when we want to send a netburst to a local
	 * server. There is a set order we must do this, because for example
	 * users require their servers to exist, and channels require their
	 * users to exist. You get the idea.
	 */
	void DoBurst(TreeServer* s);

	/** This function is called when we receive data from a remote
	 * server. We buffer the data in a std::string (it doesnt stay
	 * there for long), reading using InspSocket::Read() which can
	 * read up to 16 kilobytes in one operation.
	 *
	 * IF THIS FUNCTION RETURNS FALSE, THE CORE CLOSES AND DELETES
	 * THE SOCKET OBJECT FOR US.
	 */
	virtual bool OnDataReady();

	int WriteLine(std::string line);

	/* Handle ERROR command */
	bool Error(std::deque<std::string> &params);

	/** remote MOTD. leet, huh? */
	bool Motd(const std::string &prefix, std::deque<std::string> &params);

	/** remote ADMIN. leet, huh? */
	bool Admin(const std::string &prefix, std::deque<std::string> &params);

	bool Stats(const std::string &prefix, std::deque<std::string> &params);

	/** Because the core won't let users or even SERVERS set +o,
	 * we use the OPERTYPE command to do this.
	 */
	bool OperType(const std::string &prefix, std::deque<std::string> &params);

	/** Because Andy insists that services-compatible servers must
	 * implement SVSNICK and SVSJOIN, that's exactly what we do :p
	 */
	bool ForceNick(const std::string &prefix, std::deque<std::string> &params);

	/*
	 * Remote SQUIT (RSQUIT). Routing works similar to SVSNICK: Route it to the server that the target is connected to locally,
	 * then let that server do the dirty work (squit it!). Example:
	 * A -> B -> C -> D: oper on A squits D, A routes to B, B routes to C, C notices D connected locally, kills it. -- w00t
	 */
	bool RemoteSquit(const std::string &prefix, std::deque<std::string> &params);

	bool ServiceJoin(const std::string &prefix, std::deque<std::string> &params);

	bool RemoteRehash(const std::string &prefix, std::deque<std::string> &params);

	bool RemoteKill(const std::string &prefix, std::deque<std::string> &params);

	bool LocalPong(const std::string &prefix, std::deque<std::string> &params);

	bool MetaData(const std::string &prefix, std::deque<std::string> &params);

	bool ServerVersion(const std::string &prefix, std::deque<std::string> &params);

	bool ChangeHost(const std::string &prefix, std::deque<std::string> &params);

	bool AddLine(const std::string &prefix, std::deque<std::string> &params);

	bool ChangeName(const std::string &prefix, std::deque<std::string> &params);

	bool Whois(const std::string &prefix, std::deque<std::string> &params);

	bool Push(const std::string &prefix, std::deque<std::string> &params);

	bool HandleSetTime(const std::string &prefix, std::deque<std::string> &params);

	bool Time(const std::string &prefix, std::deque<std::string> &params);

	bool LocalPing(const std::string &prefix, std::deque<std::string> &params);

	bool RemoveStatus(const std::string &prefix, std::deque<std::string> &params);

	bool RemoteServer(const std::string &prefix, std::deque<std::string> &params);

	bool Outbound_Reply_Server(std::deque<std::string> &params);

	bool Inbound_Server(std::deque<std::string> &params);

	void Split(const std::string &line, std::deque<std::string> &n);

	bool ProcessLine(std::string &line);

	virtual std::string GetName();

	virtual void OnTimeout();

	virtual void OnClose();

	virtual int OnIncomingConnection(int newsock, char* ip);
};

#endif
