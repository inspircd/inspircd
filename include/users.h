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

#ifndef __USERS_H__
#define __USERS_H__

#include <string>
#include "inspircd_config.h"
#include "socket.h"
#include "channels.h"
#include "inspstring.h"
#include "connection.h"
#include "hashcomp.h"
#include "dns.h"

/** Channel status for a user
 */
enum ChanStatus {
	/** Op */
	STATUS_OP     = 4,
	/** Halfop */
	STATUS_HOP    = 2,
	/** Voice */
	STATUS_VOICE  = 1,
	/** None */
	STATUS_NORMAL = 0
};

/** connect class types
 */
enum ClassTypes {
	/** connect:allow */
	CC_ALLOW = 0,
	/** connect:deny */
	CC_DENY  = 1
};

/** RFC1459 channel modes
 */
enum UserModes {
	/** +s: Server notices */
	UM_SERVERNOTICE = 's' - 65,
	/** +w: WALLOPS */
	UM_WALLOPS = 'w' - 65,
	/** +i: Invisible */
	UM_INVISIBLE = 'i' - 65,
	/** +o: Operator */
	UM_OPERATOR = 'o' - 65,
	/** +n: Server notice mask */
	UM_SNOMASK = 'n' - 65
};

/** Registration state of a user, e.g.
 * have they sent USER, NICK, PASS yet?
 */
enum RegistrationState {

#ifndef WIN32   // Burlex: This is already defined in win32, luckily it is still 0.
	REG_NONE = 0,		/* Has sent nothing */
#endif

	REG_USER = 1,		/* Has sent USER */
	REG_NICK = 2,		/* Has sent NICK */
	REG_NICKUSER = 3, 	/* Bitwise combination of REG_NICK and REG_USER */
	REG_ALL = 7	  	/* REG_NICKUSER plus next bit along */
};

/* Required forward declaration */
class InspIRCd;

/** Derived from Resolver, and performs user forward/reverse lookups.
 */
class CoreExport UserResolver : public Resolver
{
 private:
	/** User this class is 'attached' to.
	 */
	userrec* bound_user;
	/** File descriptor teh lookup is bound to
	 */
	int bound_fd;
	/** True if the lookup is forward, false if is a reverse lookup
	 */
	bool fwd;
 public:
	/** Create a resolver.
	 * @param Instance The creating instance
	 * @param user The user to begin lookup on
	 * @param to_resolve The IP or host to resolve
	 * @param qt The query type
	 * @param cache Modified by the constructor if the result was cached
	 */
	UserResolver(InspIRCd* Instance, userrec* user, std::string to_resolve, QueryType qt, bool &cache);

	/** Called on successful lookup
	 * @param result Result string
	 * @param ttl Time to live for result
	 * @param cached True if the result was found in the cache
	 */
	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached);

	/** Called on failed lookup
	 * @param e Error code
	 * @param errormessage Error message string
	 */
	void OnError(ResolverError e, const std::string &errormessage);
};


/** Holds information relevent to &lt;connect allow&gt; and &lt;connect deny&gt; tags in the config file.
 */
class CoreExport ConnectClass : public classbase
{
 private:
	/** Type of line, either CC_ALLOW or CC_DENY
	 */
	char type;
	/** Max time to register the connection in seconds
	 */
	unsigned int registration_timeout;
	/** Number of lines in buffer before excess flood is triggered
	 */
	unsigned int flood;
	/** Host mask for this line
	 */
	std::string host;
	/** Number of seconds between pings for this line
	 */
	unsigned int pingtime;
	/** (Optional) Password for this line
	 */
	std::string pass;

	/** Threshold value for flood disconnect
	 */
	unsigned int threshold;

	/** Maximum size of sendq for users in this class (bytes)
	 */
	unsigned long sendqmax;

	/** Maximum size of recvq for users in this class (bytes)
	 */
	unsigned long recvqmax;

	/** Local max when connecting by this connection class
	 */
	unsigned long maxlocal;

	/** Global max when connecting by this connection class
	 */
	unsigned long maxglobal;
	/** Port number this connect class applies to
	 */
	int port;

public:

	/** Create a new connect class with no settings.
	 */
	ConnectClass() : type(CC_DENY), registration_timeout(0), flood(0), host(""), pingtime(0), pass(""),
			threshold(0), sendqmax(0), recvqmax(0), maxlocal(0), maxglobal(0) { }

	/** Create a new connect class to ALLOW connections.
	 * @param timeout The registration timeout
	 * @param fld The flood value
	 * @param hst The IP mask to allow
	 * @param ping The ping frequency
	 * @param pas The password to be used
	 * @param thres The flooding threshold
	 * @param sendq The maximum sendq value
	 * @param recvq The maximum recvq value
	 * @param maxl The maximum local sessions
	 * @param maxg The maximum global sessions
	 */
	ConnectClass(unsigned int timeout, unsigned int fld, const std::string &hst, unsigned int ping,
			const std::string &pas, unsigned int thres, unsigned long sendq, unsigned long recvq,
			unsigned long maxl, unsigned long maxg, int p = 0) :
			type(CC_ALLOW), registration_timeout(timeout), flood(fld), host(hst), pingtime(ping), pass(pas),
			threshold(thres), sendqmax(sendq), recvqmax(recvq), maxlocal(maxl), maxglobal(maxg), port(p) { }

	/** Create a new connect class to DENY  connections
	 * @param hst The IP mask to deny
	 */
	ConnectClass(const std::string &hst) : type(CC_DENY), registration_timeout(0), flood(0), host(hst), pingtime(0),
			pass(""), threshold(0), sendqmax(0), recvqmax(0), maxlocal(0), maxglobal(0), port(0) { }

	/** Returns the type, CC_ALLOW or CC_DENY
	 */
	char GetType()
	{
		return (type == CC_ALLOW ? CC_ALLOW : CC_DENY);
	}

	/** Returns the registration timeout
	 */
	unsigned int GetRegTimeout()
	{
		return (registration_timeout ? registration_timeout : 90);
	}

	/** Returns the flood limit
	 */
	unsigned int GetFlood()
	{
		return (threshold ? flood : 999);
	}

	/** Returns the allowed or denied IP mask
	 */
	const std::string& GetHost()
	{
		return host;
	}

	int GetPort()
	{
		return port;
	}

	/** Returns the ping frequency
	 */
	unsigned int GetPingTime()
	{
		return (pingtime ? pingtime : 120);
	}

	/** Returns the password or an empty string
	 */
	const std::string& GetPass()
	{
		return pass;
	}

	/** Returns the flood threshold value
	 */
	unsigned int GetThreshold()
	{
		return (threshold ? threshold : 1);
	}

	/** Returns the maximum sendq value
	 */
	unsigned long GetSendqMax()
	{
		return (sendqmax ? sendqmax : 262114);
	}

	/** Returns the maximum recvq value
	 */
	unsigned long GetRecvqMax()
	{
		return (recvqmax ? recvqmax : 4096);
	}

	/** Returusn the maximum number of local sessions
	 */
	unsigned long GetMaxLocal()
	{
		return maxlocal;
	}

	/** Returns the maximum number of global sessions
	 */
	unsigned long GetMaxGlobal()
	{
		return maxglobal;
	}
};

/** Holds a complete list of all channels to which a user has been invited and has not yet joined.
 */
typedef std::vector<irc::string> InvitedList;

/** Holds a complete list of all allow and deny tags from the configuration file (connection classes)
 */
typedef std::vector<ConnectClass> ClassVector;

/** Typedef for the list of user-channel records for a user
 */
typedef std::map<chanrec*, char> UserChanList;

/** Shorthand for an iterator into a UserChanList
 */
typedef UserChanList::iterator UCListIter;

/* Required forward declaration
 */
class userrec;

/** Visibility data for a user.
 * If a user has a non-null instance of this class in their userrec,
 * then it is used to determine if this user is visible to other users
 * or not.
 */
class CoreExport VisData
{
 public:
	/** Create a visdata
	 */
	VisData();
	/** Destroy a visdata
	 */
	virtual ~VisData();
	/** Is this user visible to some other user?
	 * @param user The other user to compare to
	 * @return true True if the user is visible to the other user, false if not
	 */
	virtual bool VisibleTo(userrec* user);
};

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname. Use the FindNick method of the InspIRCd class to locate a specific user
 * by nickname, or the FindDescriptor method of the InspIRCd class to find a specific user by their
 * file descriptor value.
 */
class CoreExport userrec : public connection
{
 private:
	/** Pointer to creator.
	 * This is required to make use of core functions
	 * from within the userrec class.
	 */
	InspIRCd* ServerInstance;

	/** A list of channels the user has a pending invite to.
	 * Upon INVITE channels are added, and upon JOIN, the
	 * channels are removed from this list.
	 */
	InvitedList invites;

	/** Number of channels this user is currently on
	 */
	unsigned int ChannelCount;

	/** Cached nick!ident@host value using the real hostname
	 */
	char* cached_fullhost;

	/** Cached nick!ident@ip value using the real IP address
	 */
	char* cached_hostip;

	/** Cached nick!ident@host value using the masked hostname
	 */
	char* cached_makehost;

	/** Cached nick!ident@realhost value using the real hostname
	 */
	char* cached_fullrealhost;

	/** When we erase the user (in the destructor),
	 * we call this method to subtract one from all
	 * mode characters this user is making use of.
	 */
	void DecrementModes();

	/** Oper-only quit message for this user if non-null
	 */
	char* operquit;

 public:
	/** Resolvers for looking up this users IP address
	 * This will occur if and when res_reverse completes.
	 * When this class completes its lookup, userrec::dns_done
	 * will be set from false to true.
	 */
	UserResolver* res_forward;

	/** Resolvers for looking up this users hostname
	 * This is instantiated by userrec::StartDNSLookup(),
	 * and on success, instantiates userrec::res_reverse.
	 */
	UserResolver* res_reverse;

	/** User visibility state, see definition of VisData.
	 */
	VisData* Visibility;

	/** Stored reverse lookup from res_forward
	 */
	std::string stored_host;

	/** Starts a DNS lookup of the user's IP.
	 * This will cause two UserResolver classes to be instantiated.
	 * When complete, these objects set userrec::dns_done to true.
	 */
	void StartDNSLookup();

	/** The users nickname.
	 * An invalid nickname indicates an unregistered connection prior to the NICK command.
	 * Use InspIRCd::IsNick() to validate nicknames.
	 */
	char nick[NICKMAX];

	/** The users ident reply.
	 * Two characters are added to the user-defined limit to compensate for the tilde etc.
	 */
	char ident[IDENTMAX+2];

	/** The host displayed to non-opers (used for cloaking etc).
	 * This usually matches the value of userrec::host.
	 */
	char dhost[65];

	/** The users full name (GECOS).
	 */
	char fullname[MAXGECOS+1];

	/** The user's mode list.
	 * This is NOT a null terminated string! In the 1.1 version of InspIRCd
	 * this is an array of values in a similar way to channel modes.
	 * A value of 1 in field (modeletter-65) indicates that the mode is
	 * set, for example, to work out if mode +s is set, we  check the field
	 * userrec::modes['s'-65] != 0.
	 * The following RFC characters o, w, s, i have constants defined via an
	 * enum, such as UM_SERVERNOTICE and UM_OPETATOR.
	 */
	char modes[64];

	/** What snomasks are set on this user.
	 * This functions the same as the above modes.
	 */
	char snomasks[64];

	/** Channels this user is on, and the permissions they have there
	 */
	UserChanList chans;

	/** The server the user is connected to.
	 */
	const char* server;

	/** The user's away message.
	 * If this string is empty, the user is not marked as away.
	 */
	char awaymsg[MAXAWAY+1];

	/** Number of lines the user can place into the buffer
	 * (up to the global NetBufferSize bytes) before they
	 * are disconnected for excess flood
	 */
	int flood;

	/** Timestamp of current time + connection class timeout.
	 * This user must send USER/NICK before this timestamp is
	 * reached or they will be disconnected.
	 */
	time_t timeout;

	/** The oper type they logged in as, if they are an oper.
	 * This is used to check permissions in operclasses, so that
	 * we can say 'yay' or 'nay' to any commands they issue.
	 * The value of this is the value of a valid 'type name=' tag.
	 */
	char oper[NICKMAX];

	/** True when DNS lookups are completed.
	 * The UserResolver classes res_forward and res_reverse will
	 * set this value once they complete.
	 */
	bool dns_done;

	/** Number of seconds between PINGs for this user (set from &lt;connect:allow&gt; tag
	 */
	unsigned int pingmax;

	/** Password specified by the user when they registered.
	 * This is stored even if the <connect> block doesnt need a password, so that
	 * modules may check it.
	 */
	char password[64];

	/** User's receive queue.
	 * Lines from the IRCd awaiting processing are stored here.
	 * Upgraded april 2005, old system a bit hairy.
	 */
	std::string recvq;

	/** User's send queue.
	 * Lines waiting to be sent are stored here until their buffer is flushed.
	 */
	std::string sendq;

	/** Flood counters - lines received
	 */
	int lines_in;

	/** Flood counters - time lines_in is due to be reset
	 */
	time_t reset_due;

	/** Flood counters - Highest value lines_in may reach before the user gets disconnected
	 */
	long threshold;

	/** If this is set to true, then all read operations for the user
	 * are dropped into the bit-bucket.
	 * This is used by the global CullList, but please note that setting this value
	 * alone will NOT cause the user to quit. This means it can be used seperately,
	 * for example by shun modules etc.
	 */
	bool muted;

	/** IPV4 or IPV6 ip address. Use SetSockAddr to set this and GetProtocolFamily/
	 * GetIPString/GetPort to obtain its values.
	 */
	sockaddr* ip;

	/** Initialize the clients sockaddr
	 * @param protocol_family The protocol family of the IP address, AF_INET or AF_INET6
	 * @param ip A human-readable IP address for this user matching the protcol_family
	 * @param port The port number of this user or zero for a remote user
	 */
	void SetSockAddr(int protocol_family, const char* ip, int port);

	/** Get port number from sockaddr
	 * @return The port number of this user.
	 */
	int GetPort();

	/** Get protocol family from sockaddr
	 * @return The protocol family of this user, either AF_INET or AF_INET6
	 */
	int GetProtocolFamily();

	/** Get IP string from sockaddr, using static internal buffer
	 * @return The IP string
	 */
	const char* GetIPString();

	/** Get IP string from sockaddr, using caller-specified buffer
	 * @param buf A buffer to use
	 * @return The IP string
	 */
	const char* GetIPString(char* buf);

	/* Write error string
	 */
	std::string WriteError;

	/** Maximum size this user's sendq can become.
	 * Copied from the connect class on connect.
	 */
	long sendqmax;

	/** Maximum size this user's recvq can become.
	 * Copied from the connect class on connect.
	 */
	long recvqmax;

	/** This is true if the user matched an exception when they connected to the ircd.
	 * It isnt valid after this point, and you should not attempt to do anything with it
	 * after this point, because the eline might be removed at a later time, and/or no
	 * longer be applicable to this user. It is only used to save doing the eline lookup
	 * twice (instead we do it once and set this value).
	 */
	bool exempt;

	/** Default constructor
	 * @throw Nothing at present
	 */
	userrec(InspIRCd* Instance);

	/** Returns the full displayed host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident&at;host form.
	 * @return The full masked host of the user
	 */
	virtual char* GetFullHost();

	/** Returns the full real host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident&at;host form. If any form of hostname cloaking is in operation,
	 * e.g. through a module, then this method will ignore it and return the true hostname.
	 * @return The full real host of the user
	 */
	virtual char* GetFullRealHost();

	/** This clears any cached results that are used for GetFullRealHost() etc.
	 * The results of these calls are cached as generating them can be generally expensive.
	 */
	void InvalidateCache();

	/** Create a displayable mode string for this users snomasks
	 * @return The notice mask character sequence
	 */
	const char* FormatNoticeMasks();

	/** Process a snomask modifier string, e.g. +abc-de
	 * @param sm A sequence of notice mask characters
	 * @return The cleaned mode sequence which can be output,
	 * e.g. in the above example if masks c and e are not
	 * valid, this function will return +ab-d
	 */
	std::string ProcessNoticeMasks(const char *sm);

	/** Returns true if a notice mask is set
	 * @param sm A notice mask character to check
	 * @return True if the notice mask is set
	 */
	bool IsNoticeMaskSet(unsigned char sm);

	/** Changed a specific notice mask value
	 * @param sm The server notice mask to change
	 * @param value An on/off value for this mask
	 */
	void SetNoticeMask(unsigned char sm, bool value);

	/** Create a displayable mode string for this users umodes
	 * @param The mode string
	 */
	const char* FormatModes();

	/** Returns true if a specific mode is set
	 * @param m The user mode
	 * @return True if the mode is set
	 */
	bool IsModeSet(unsigned char m);

	/** Set a specific usermode to on or off
	 * @param m The user mode
	 * @param value On or off setting of the mode
	 */
	void SetMode(unsigned char m, bool value);

	/** Returns true if a user is invited to a channel.
	 * @param channel A channel name to look up
	 * @return True if the user is invited to the given channel
	 */
	virtual bool IsInvited(const irc::string &channel);

	/** Adds a channel to a users invite list (invites them to a channel)
	 * @param channel A channel name to add
	 */
	virtual void InviteTo(const irc::string &channel);

	/** Removes a channel from a users invite list.
	 * This member function is called on successfully joining an invite only channel
	 * to which the user has previously been invited, to clear the invitation.
	 * @param channel The channel to remove the invite to
	 */
	virtual void RemoveInvite(const irc::string &channel);

	/** Returns true or false for if a user can execute a privilaged oper command.
	 * This is done by looking up their oper type from userrec::oper, then referencing
	 * this to their oper classes and checking the commands they can execute.
	 * @param command A command (should be all CAPS)
	 * @return True if this user can execute the command
	 */
	bool HasPermission(const std::string &command);

	/** Calls read() to read some data for this user using their fd.
	 * @param buffer The buffer to read into
	 * @param size The size of data to read
	 * @return The number of bytes read, or -1 if an error occured.
	 */
	int ReadData(void* buffer, size_t size);

	/** This method adds data to the read buffer of the user.
	 * The buffer can grow to any size within limits of the available memory,
	 * managed by the size of a std::string, however if any individual line in
	 * the buffer grows over 600 bytes in length (which is 88 chars over the
	 * RFC-specified limit per line) then the method will return false and the
	 * text will not be inserted.
	 * @param a The string to add to the users read buffer
	 * @return True if the string was successfully added to the read buffer
	 */
	bool AddBuffer(std::string a);

	/** This method returns true if the buffer contains at least one carriage return
	 * character (e.g. one complete line may be read)
	 * @return True if there is at least one complete line in the users buffer
	 */
	bool BufferIsReady();

	/** This function clears the entire buffer by setting it to an empty string.
	 */
	void ClearBuffer();

	/** This method returns the first available string at the tail end of the buffer
	 * and advances the tail end of the buffer past the string. This means it is
	 * a one way operation in a similar way to strtok(), and multiple calls return
	 * multiple lines if they are available. The results of this function if there
	 * are no lines to be read are unknown, always use BufferIsReady() to check if
	 * it is ok to read the buffer before calling GetBuffer().
	 * @return The string at the tail end of this users buffer
	 */
	std::string GetBuffer();

	/** Sets the write error for a connection. This is done because the actual disconnect
	 * of a client may occur at an inopportune time such as half way through /LIST output.
	 * The WriteErrors of clients are checked at a more ideal time (in the mainloop) and
	 * errored clients purged.
	 * @param error The error string to set.
	 */
	void SetWriteError(const std::string &error);

	/** Returns the write error which last occured on this connection or an empty string
	 * if none occured.
	 * @return The error string which has occured for this user
	 */
	const char* GetWriteError();

	/** Adds to the user's write buffer.
	 * You may add any amount of text up to this users sendq value, if you exceed the
	 * sendq value, SetWriteError() will be called to set the users error string to
	 * "SendQ exceeded", and further buffer adds will be dropped.
	 * @param data The data to add to the write buffer
	 */
	void AddWriteBuf(const std::string &data);

	/** Flushes as much of the user's buffer to the file descriptor as possible.
	 * This function may not always flush the entire buffer, rather instead as much of it
	 * as it possibly can. If the send() call fails to send the entire buffer, the buffer
	 * position is advanced forwards and the rest of the data sent at the next call to
	 * this method.
	 */
	void FlushWriteBuf();

	/** Returns the list of channels this user has been invited to but has not yet joined.
	 * @return A list of channels the user is invited to
	 */
	InvitedList* GetInviteList();

	/** Creates a wildcard host.
	 * Takes a buffer to use and fills the given buffer with the host in the format *!*@hostname
	 * @return The wildcarded hostname in *!*@host form
	 */
	char* MakeWildHost();

	/** Creates a usermask with real host.
	 * Takes a buffer to use and fills the given buffer with the hostmask in the format user@host
	 * @return the usermask in the format user@host
	 */
	char* MakeHost();

	/** Creates a usermask with real ip.
	 * Takes a buffer to use and fills the given buffer with the ipmask in the format user@ip
	 * @return the usermask in the format user@ip
	 */
	char* MakeHostIP();

	/** Shuts down and closes the user's socket
	 * This will not cause the user to be deleted. Use InspIRCd::QuitUser for this,
	 * which will call CloseSocket() for you.
	 */
	void CloseSocket();

	/** Disconnect a user gracefully
	 * @param user The user to remove
	 * @param r The quit reason to show to normal users
	 * @param oreason The quit reason to show to opers
	 * @return Although this function has no return type, on exit the user provided will no longer exist.
	 */
	static void QuitUser(InspIRCd* Instance, userrec *user, const std::string &r, const char* oreason = "");

	/** Add the user to WHOWAS system
	 */
	void AddToWhoWas();

	/** Oper up the user using the given opertype.
	 * This will also give the +o usermode.
	 * @param opertype The oper type to oper as
	 */
	void Oper(const std::string &opertype);

	/** Call this method to find the matching <connect> for a user, and to check them against it.
	 */
	void CheckClass();

	/** Use this method to fully connect a user.
	 * This will send the message of the day, check G/K/E lines, etc.
	 */
	void FullConnect();

	/** Change this users hash key to a new string.
	 * You should not call this function directly. It is used by the core
	 * to update the users hash entry on a nickchange.
	 * @param New new user_hash key
	 * @return Pointer to userrec in hash (usually 'this')
	 */
	userrec* UpdateNickHash(const char* New);

	/** Force a nickname change.
	 * If the nickname change fails (for example, because the nick in question
	 * already exists) this function will return false, and you must then either
	 * output an error message, or quit the user for nickname collision.
	 * @param newnick The nickname to change to
	 * @return True if the nickchange was successful.
	 */
	bool ForceNickChange(const char* newnick);

	/** Add a client to the system.
	 * This will create a new userrec, insert it into the user_hash,
	 * initialize it as not yet registered, and add it to the socket engine.
	 * @param Instance a pointer to the server instance
	 * @param socket The socket id (file descriptor) this user is on
	 * @param port The port number this user connected on
	 * @param iscached This variable is reserved for future use
	 * @param ip The IP address of the user
	 * @return This function has no return value, but a call to AddClient may remove the user.
	 */
	static void AddClient(InspIRCd* Instance, int socket, int port, bool iscached, int socketfamily, sockaddr* ip);

	/** Oper down.
	 * This will clear the +o usermode and unset the user's oper type
	 */
	void UnOper();

	/** Return the number of global clones of this user
	 * @return The global clone count of this user
	 */
	unsigned long GlobalCloneCount();

	/** Return the number of local clones of this user
	 * @return The local clone count of this user
	 */
	unsigned long LocalCloneCount();

	/** Remove all clone counts from the user, you should
	 * use this if you change the user's IP address in
	 * userrec::ip after they have registered.
	 */
	void RemoveCloneCounts();

	/** Write text to this user, appending CR/LF.
	 * @param text A std::string to send to the user
	 */
	void Write(std::string text);

	/** Write text to this user, appending CR/LF.
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void Write(const char *text, ...);

	/** Write text to this user, appending CR/LF and prepending :server.name
	 * @param text A std::string to send to the user
	 */
	void WriteServ(const std::string& text);

	/** Write text to this user, appending CR/LF and prepending :server.name
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteServ(const char* text, ...);

	/** Write text to this user, appending CR/LF and prepending :nick!user@host of the user provided in the first parameter.
	 * @param user The user to prepend the :nick!user@host of
	 * @param text A std::string to send to the user
	 */
	void WriteFrom(userrec *user, const std::string &text);

	/** Write text to this user, appending CR/LF and prepending :nick!user@host of the user provided in the first parameter.
	 * @param user The user to prepend the :nick!user@host of
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteFrom(userrec *user, const char* text, ...);

	/** Write text to the user provided in the first parameter, appending CR/LF, and prepending THIS user's :nick!user@host.
	 * @param dest The user to route the message to
	 * @param text A std::string to send to the user
	 */
	void WriteTo(userrec *dest, const std::string &data);

	/** Write text to the user provided in the first parameter, appending CR/LF, and prepending THIS user's :nick!user@host.
	 * @param dest The user to route the message to
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteTo(userrec *dest, const char *data, ...);

	/** Write to all users that can see this user (including this user in the list), appending CR/LF
	 * @param text A std::string to send to the users
	 */
	void WriteCommon(const std::string &text);

	/** Write to all users that can see this user (including this user in the list), appending CR/LF
	 * @param text The format string for text to send to the users
	 * @param ... POD-type format arguments
	 */
	void WriteCommon(const char* text, ...);

	/** Write to all users that can see this user (not including this user in the list), appending CR/LF
	 * @param text The format string for text to send to the users
	 * @param ... POD-type format arguments
	 */
	void WriteCommonExcept(const char* text, ...);

	/** Write to all users that can see this user (not including this user in the list), appending CR/LF
	 * @param text A std::string to send to the users
	 */
	void WriteCommonExcept(const std::string &text);

	/** Write a quit message to all common users, as in userrec::WriteCommonExcept but with a specific
	 * quit message for opers only.
	 * @param normal_text Normal user quit message
	 * @param oper_text Oper only quit message
	 */
	void WriteCommonQuit(const std::string &normal_text, const std::string &oper_text);

	/** Write a WALLOPS message from this user to all local opers.
	 * If this user is not opered, the function will return without doing anything.
	 * @param text The format string to send in the WALLOPS message
	 * @param ... Format arguments
	 */
	void WriteWallOps(const char* text, ...);

	/** Write a WALLOPS message from this user to all local opers.
	 * If this user is not opered, the function will return without doing anything.
	 * @param text The text to send in the WALLOPS message
	 */
	void WriteWallOps(const std::string &text);

	/** Return true if the user shares at least one channel with another user
	 * @param other The other user to compare the channel list against
	 * @return True if the given user shares at least one channel with this user
	 */
	bool SharesChannelWith(userrec *other);

	/** Change the displayed host of a user.
	 * ALWAYS use this function, rather than writing userrec::dhost directly,
	 * as this triggers module events allowing the change to be syncronized to
	 * remote servers. This will also emulate a QUIT and rejoin (where configured)
	 * before setting their host field.
	 * @param host The new hostname to set
	 * @return True if the change succeeded, false if it didn't
	 */
	bool ChangeDisplayedHost(const char* host);

	/** Change the ident (username) of a user.
	 * ALWAYS use this function, rather than writing userrec::ident directly,
	 * as this correctly causes the user to seem to quit (where configured)
	 * before setting their ident field.
	 * @param host The new ident to set
	 * @return True if the change succeeded, false if it didn't
	 */
	bool ChangeIdent(const char* newident);

	/** Change a users realname field.
	 * ALWAYS use this function, rather than writing userrec::fullname directly,
	 * as this triggers module events allowing the change to be syncronized to
	 * remote servers.
	 * @param gecos The user's new realname
	 * @return True if the change succeeded, false if otherwise
	 */
	bool ChangeName(const char* gecos);

	/** Send a command to all local users from this user
	 * The command given must be able to send text with the
	 * first parameter as a servermask (e.g. $*), so basically
	 * you should use PRIVMSG or NOTICE.
	 * @param command the command to send
	 * @param text The text format string to send
	 * @param ... Format arguments
	 */
	void SendAll(const char* command, char* text, ...);

	/** Compile a channel list for this user, and send it to the user 'source'
	 * Used internally by WHOIS
	 * @param The user to send the channel list to if it is not too long
	 * @return This user's channel list
	 */
	std::string ChannelList(userrec* source);

	/** Split the channel list in cl which came from dest, and spool it to this user
	 * Used internally by WHOIS
	 * @param dest The user the original channel list came from
	 * @param cl The  channel list as a string obtained from userrec::ChannelList()
	 */
	void SplitChanList(userrec* dest, const std::string &cl);

	/** Remove this user from all channels they are on, and delete any that are now empty.
	 * This is used by QUIT, and will not send part messages!
	 */
	void PurgeEmptyChannels();

	/** Get the connect class which matches this user's host or IP address
	 * @return A reference to this user's connect class
	 */
	ConnectClass* GetClass();

	/** Show the message of the day to this user
	 */
	void ShowMOTD();

	/** Show the server RULES file to this user
	 */
	void ShowRULES();

	/** Set oper-specific quit message shown to opers only when the user quits
	 * (overrides any sent by QuitUser)
	 */
	void SetOperQuit(const std::string &oquit);

	/** Get oper-specific quit message shown only to opers when the user quits.
	 * (overrides any sent by QuitUser)
	 */
	const char* GetOperQuit();

	/** Handle socket event.
	 * From EventHandler class.
	 * @param et Event type
	 * @param errornum Error number for EVENT_ERROR events
	 */
	void HandleEvent(EventType et, int errornum = 0);

	/** Default destructor
	 */
	virtual ~userrec();
};

/* Configuration callbacks */
class ServerConfig;

#endif

