/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2003-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Burlex <???@???>
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


#ifndef __USERS_H__
#define __USERS_H__

#include "socket.h"
#include "dns.h"
#include "mode.h"

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
	/** +s: Server notice mask */
	UM_SNOMASK = 's' - 65,
	/** +w: WALLOPS */
	UM_WALLOPS = 'w' - 65,
	/** +i: Invisible */
	UM_INVISIBLE = 'i' - 65,
	/** +o: Operator */
	UM_OPERATOR = 'o' - 65
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
class Channel;
class UserResolver;

/** Holds information relevent to &lt;connect allow&gt; and &lt;connect deny&gt; tags in the config file.
 */
struct CoreExport ConnectClass : public classbase
{
	/** Type of line, either CC_ALLOW or CC_DENY
	 */
	char type;

	/** Connect class name
	 */
	std::string name;

	/** Max time to register the connection in seconds
	 */
	unsigned int registration_timeout;

	/** Host mask for this line
	 */
	std::string host;

	/** Number of seconds between pings for this line
	 */
	unsigned int pingtime;

	/** (Optional) Password for this line
	 */
	std::string pass;

	/** (Optional) Hash Method for this line
	 */
	std::string hash;

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

	/** Max channels for this class
	 */
	unsigned int maxchans;

	/** Port number this connect class applies to
	 */
	int port;

	/** How many users may be in this connect class before they are refused?
	 * (0 = no limit = default)
	 */
	unsigned long limit;

	/** Reference counter.
	 * This will be 1 if no users are connected, as long as it is a valid connect block
	 * When it reaches 0, the object should be deleted
	 */
	unsigned long RefCount;

	/** Create a new connect class with no settings.
	 */
	ConnectClass(char type, const std::string& mask);
	/** Create a new connect class with inherited settings.
	 */
	ConnectClass(char type, const std::string& mask, const ConnectClass& parent);
	
	/** Update the settings in this block to match the given block */
	void Update(const ConnectClass* newSettings);


	const std::string& GetName() { return name; }
	const std::string& GetPass() { return pass; }
	const std::string& GetHost() { return host; }
	const int GetPort() { return port; }
	
	/** Returns the registration timeout
	 */
	time_t GetRegTimeout()
	{
		return (registration_timeout ? registration_timeout : 90);
	}

	/** Returns the ping frequency
	 */
	unsigned int GetPingTime()
	{
		return (pingtime ? pingtime : 120);
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

/** Holds a complete list of all channels to which a user has been invited and has not yet joined, and the time at which they'll expire.
 */
typedef std::vector< std::pair<irc::string, time_t> > InvitedList;

/** Holds a complete list of all allow and deny tags from the configuration file (connection classes)
 */
typedef std::vector<ConnectClass*> ClassVector;

/** Typedef for the list of user-channel records for a user
 */
typedef std::map<Channel*, char> UserChanList;

/** Shorthand for an iterator into a UserChanList
 */
typedef UserChanList::iterator UCListIter;

/* Required forward declaration
 */
class User;

/** Visibility data for a user.
 * If a user has a non-null instance of this class in their User,
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
	virtual bool VisibleTo(User* user);
};

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname.
 */
class CoreExport User : public EventHandler
{
 private:
	/** A list of channels the user has a pending invite to.
	 * Upon INVITE channels are added, and upon JOIN, the
	 * channels are removed from this list.
	 */
	InvitedList invites;

	/** Cached nick!ident@dhost value using the displayed hostname
	 */
	std::string cached_fullhost;

	/** Cached ident@ip value using the real IP address
	 */
	std::string cached_hostip;

	/** Cached ident@realhost value using the real hostname
	 */
	std::string cached_makehost;

	/** Cached nick!ident@realhost value using the real hostname
	 */
	std::string cached_fullrealhost;

	/** Set by GetIPString() to avoid constantly re-grabbing IP via sockets voodoo.
	 */
	std::string cachedip;

	/** When we erase the user (in the destructor),
	 * we call this method to subtract one from all
	 * mode characters this user is making use of.
	 */
	void DecrementModes();

	std::set<std::string> *AllowedOperCommands;
	std::set<std::string> *AllowedPrivs;

	/** Allowed user modes from oper classes. */
	std::bitset<64> AllowedUserModes;

	/** Allowed channel modes from oper classes. */
	std::bitset<64> AllowedChanModes;

 public:
	/** Pointer to creator.
	 * This is required to make use of core functions
	 * from within the User class.
	 */
	InspIRCd* ServerInstance;

	/** Contains a pointer to the connect class a user is on from - this will be NULL for remote connections.
	 * The pointer is guarenteed to *always* be valid. :)
	 */
	ConnectClass *MyClass;

	/** User visibility state, see definition of VisData.
	 */
	VisData* Visibility;

	/** Hostname of connection.
	 * This should be valid as per RFC1035.
	 */
	std::string host;

	/** Stats counter for bytes inbound
	 */
	int bytes_in;

	/** Stats counter for bytes outbound
	 */
	int bytes_out;

	/** Stats counter for commands inbound
	 */
	int cmds_in;

	/** Stats counter for commands outbound
	 */
	int cmds_out;

	/** True if user has authenticated, false if otherwise
	 */
	bool haspassed;

	/** Used by User to indicate the registration status of the connection
	 * It is a bitfield of the REG_NICK, REG_USER and REG_ALL bits to indicate
	 * the connection state.
	 */
	char registered;

	/** Time the connection was last pinged
	 */
	time_t lastping;

	/** Time the connection was created, set in the constructor. This
	 * may be different from the time the user's classbase object was
	 * created.
	 */
	time_t signon;

	/** Time that the connection last sent a message, used to calculate idle time
	 */
	time_t idle_lastmsg;

	/** Used by PING checking code
	 */
	time_t nping;

	/** Stored reverse lookup from res_forward. Should not be used after resolution.
	 */
	std::string stored_host;

	/** Starts a DNS lookup of the user's IP.
	 * This will cause two UserResolver classes to be instantiated.
	 * When complete, these objects set User::dns_done to true.
	 */
	void StartDNSLookup();

	/** The users nickname.
	 * An invalid nickname indicates an unregistered connection prior to the NICK command.
	 * Use InspIRCd::IsNick() to validate nicknames.
	 */
	std::string nick;

	/** The user's unique identifier.
	 * This is the unique identifier which the user has across the network.
	 */
	std::string uuid;

	/** The users ident reply.
	 * Two characters are added to the user-defined limit to compensate for the tilde etc.
	 */
	std::string ident;

	/** The host displayed to non-opers (used for cloaking etc).
	 * This usually matches the value of User::host.
	 */
	std::string dhost;

	/** The users full name (GECOS).
	 */
	std::string fullname;

	/** The user's mode list.
	 * NOT a null terminated string.
	 * Also NOT an array.
	 * Much love to the STL for giving us an easy to use bitset, saving us RAM.
	 * if (modes[modeletter-65]) is set, then the mode is
	 * set, for example, to work out if mode +s is set, we  check the field
	 * User::modes['s'-65] != 0.
	 * The following RFC characters o, w, s, i have constants defined via an
	 * enum, such as UM_SERVERNOTICE and UM_OPETATOR.
	 */
	std::bitset<64> modes;

	/** What snomasks are set on this user.
	 * This functions the same as the above modes.
	 */
	std::bitset<64> snomasks;

	/** Channels this user is on, and the permissions they have there
	 */
	UserChanList chans;

	/** The server the user is connected to.
	 */
	const char* server;

	/** The user's away message.
	 * If this string is empty, the user is not marked as away.
	 */
	std::string awaymsg;

	/** Time the user last went away.
	 * This is ONLY RELIABLE if user IS_AWAY()!
	 */
	time_t awaytime;

	/** The oper type they logged in as, if they are an oper.
	 * This is used to check permissions in operclasses, so that
	 * we can say 'yay' or 'nay' to any commands they issue.
	 * The value of this is the value of a valid 'type name=' tag.
	 */
	std::string oper;

	/** True when DNS lookups are completed.
	 * The UserResolver classes res_forward and res_reverse will
	 * set this value once they complete.
	 */
	bool dns_done;

	/** Password specified by the user when they registered.
	 * This is stored even if the <connect> block doesnt need a password, so that
	 * modules may check it.
	 */
	std::string password;

	/** User's receive queue.
	 * Lines from the IRCd awaiting processing are stored here.
	 * Upgraded april 2005, old system a bit hairy.
	 */
	std::string recvq;

	/** User's send queue.
	 * Lines waiting to be sent are stored here until their buffer is flushed.
	 */
	std::string sendq;

	/** Message user will quit with. Not to be set externally.
	 */
	std::string quitmsg;

	/** Quit message shown to opers - not to be set externally.
	 */
	std::string operquitmsg;

	/** Whether or not to send an snotice about this user's quitting
	 */
	bool quietquit;

	/** If this is set to true, then all socket operations for the user
	 * are dropped into the bit-bucket.
	 * This value is set by QuitUser, and is not needed seperately from that call.
	 * Please note that setting this value alone will NOT cause the user to quit.
	 */
	bool quitting;

	/** Recursion fix: user is out of SendQ and will be quit as soon as possible.
	 * This can't be handled normally because QuitUser itself calls Write on other
	 * users, which could trigger their SendQ to overrun.
	 */
	bool quitting_sendq;

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

	/** Get a CIDR mask from the IP of this user, using a static internal buffer.
	 * e.g., GetCIDRMask(16) for 223.254.214.52 returns 223.254.0.0/16
	 * This may be used for CIDR clone detection, etc.
	 *
	 * (XXX, brief note: when we do the sockets rewrite, this should move down a
	 * level so it may be used on more derived objects. -- w00t)
	 */
	const char *GetCIDRMask(int range);

	/** This is true if the user matched an exception (E:Line). It is used to save time on ban checks.
	 */
	bool exempt;

	/** This value contains how far into the penalty threshold the user is. Once its over
	 * the penalty threshold then commands are held and processed on-timer.
	 */
	int Penalty;

	/** Default constructor
	 * @throw CoreException if the UID allocated to the user already exists
	 * @param Instance Creator instance
	 * @param uid User UUID, or empty to allocate one automatically
	 */
	User(InspIRCd* Instance, const std::string &uid = "");

	/** Check if the user matches a G or K line, and disconnect them if they do.
	 * @param doZline True if ZLines should be checked (if IP has changed since initial connect)
	 * Returns true if the user matched a ban, false else.
	 */
	bool CheckLines(bool doZline = false);

	/** Returns the full displayed host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident&at;host form.
	 * @return The full masked host of the user
	 */
	virtual const std::string GetFullHost();

	/** Returns the full real host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident&at;host form. If any form of hostname cloaking is in operation,
	 * e.g. through a module, then this method will ignore it and return the true hostname.
	 * @return The full real host of the user
	 */
	virtual const std::string GetFullRealHost();

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
	const char* FormatModes(bool showparameters = false);

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
	 * @param timeout When the invite should expire (0 == never)
	 */
	virtual void InviteTo(const irc::string &channel, time_t timeout);

	/** Removes a channel from a users invite list.
	 * This member function is called on successfully joining an invite only channel
	 * to which the user has previously been invited, to clear the invitation.
	 * @param channel The channel to remove the invite to
	 */
	virtual void RemoveInvite(const irc::string &channel);

	/** Returns true or false for if a user can execute a privilaged oper command.
	 * This is done by looking up their oper type from User::oper, then referencing
	 * this to their oper classes and checking the commands they can execute.
	 * @param command A command (should be all CAPS)
	 * @return True if this user can execute the command
	 */
	bool HasPermission(const std::string &command);

	/** Returns true if a user has a given permission.
	 * This is used to check whether or not users may perform certain actions which admins may not wish to give to
	 * all operators, yet are not commands. An example might be oper override, mass messaging (/notice $*), etc.
	 *
	 * @param privstr The priv to chec, e.g. "users/override/topic". These are loaded free-form from the config file.
	 * @param noisy If set to true, the user is notified that they do not have the specified permission where applicable. If false, no notification is sent.
	 * @return True if this user has the permission in question.
	 */
	bool HasPrivPermission(const std::string &privstr, bool noisy = false);

	/** Returns true or false if a user can set a privileged user or channel mode.
	 * This is done by looking up their oper type from User::oper, then referencing
	 * this to their oper classes, and checking the modes they can set.
	 * @param mode The mode the check
	 * @param type ModeType (MODETYPE_CHANNEL or MODETYPE_USER).
	 * @return True if the user can set or unset this mode.
	 */
	bool HasModePermission(unsigned char mode, ModeType type);

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
	bool AddBuffer(const std::string &a);

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

	/** Adds to the user's write buffer.
	 * You may add any amount of text up to this users sendq value, if you exceed the
	 * sendq value, the user will be removed, and further buffer adds will be dropped.
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
	const std::string& MakeHost();

	/** Creates a usermask with real ip.
	 * Takes a buffer to use and fills the given buffer with the ipmask in the format user@ip
	 * @return the usermask in the format user@ip
	 */
	const std::string& MakeHostIP();

	/** Shuts down and closes the user's socket
	 * This will not cause the user to be deleted. Use InspIRCd::QuitUser for this,
	 * which will call CloseSocket() for you.
	 */
	void CloseSocket();

	/** Add the user to WHOWAS system
	 */
	void AddToWhoWas();

	/** Oper up the user using the given opertype.
	 * This will also give the +o usermode.
	 * @param opertype The oper type to oper as
	 */
	void Oper(const std::string &opertype, const std::string &opername);

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
	 * @return Pointer to User in hash (usually 'this')
	 */
	User* UpdateNickHash(const char* New);

	/** Force a nickname change.
	 * If the nickname change fails (for example, because the nick in question
	 * already exists) this function will return false, and you must then either
	 * output an error message, or quit the user for nickname collision.
	 * @param newnick The nickname to change to
	 * @return True if the nickchange was successful.
	 */
	bool ForceNickChange(const char* newnick);

	/** Oper down.
	 * This will clear the +o usermode and unset the user's oper type
	 */
	void UnOper();

	/** Write text to this user, appending CR/LF.
	 * @param text A std::string to send to the user
	 */
	void Write(std::string text);

	/** Write text to this user, appending CR/LF.
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void Write(const char *text, ...) CUSTOM_PRINTF(2, 3);

	/** Write text to this user, appending CR/LF and prepending :server.name
	 * @param text A std::string to send to the user
	 */
	void WriteServ(const std::string& text);

	/** Write text to this user, appending CR/LF and prepending :server.name
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteServ(const char* text, ...) CUSTOM_PRINTF(2, 3);

	void WriteNumeric(unsigned int numeric, const char* text, ...) CUSTOM_PRINTF(3, 4);

	void WriteNumeric(unsigned int numeric, const std::string &text);

	/** Write text to this user, appending CR/LF and prepending :nick!user@host of the user provided in the first parameter.
	 * @param user The user to prepend the :nick!user@host of
	 * @param text A std::string to send to the user
	 */
	void WriteFrom(User *user, const std::string &text);

	/** Write text to this user, appending CR/LF and prepending :nick!user@host of the user provided in the first parameter.
	 * @param user The user to prepend the :nick!user@host of
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteFrom(User *user, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Write text to the user provided in the first parameter, appending CR/LF, and prepending THIS user's :nick!user@host.
	 * @param dest The user to route the message to
	 * @param text A std::string to send to the user
	 */
	void WriteTo(User *dest, const std::string &data);

	/** Write text to the user provided in the first parameter, appending CR/LF, and prepending THIS user's :nick!user@host.
	 * @param dest The user to route the message to
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteTo(User *dest, const char *data, ...) CUSTOM_PRINTF(3, 4);

	/** Write to all users that can see this user (including this user in the list), appending CR/LF
	 * @param text A std::string to send to the users
	 */
	void WriteCommon(const std::string &text);

	/** Write to all users that can see this user (including this user in the list), appending CR/LF
	 * @param text The format string for text to send to the users
	 * @param ... POD-type format arguments
	 */
	void WriteCommon(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Write to all users that can see this user (not including this user in the list), appending CR/LF
	 * @param text The format string for text to send to the users
	 * @param ... POD-type format arguments
	 */
	void WriteCommonExcept(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Write to all users that can see this user (not including this user in the list), appending CR/LF
	 * @param text A std::string to send to the users
	 */
	void WriteCommonExcept(const std::string &text);

	/** Write a quit message to all common users, as in User::WriteCommonExcept but with a specific
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
	void WriteWallOps(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Write a WALLOPS message from this user to all local opers.
	 * If this user is not opered, the function will return without doing anything.
	 * @param text The text to send in the WALLOPS message
	 */
	void WriteWallOps(const std::string &text);

	/** Return true if the user shares at least one channel with another user
	 * @param other The other user to compare the channel list against
	 * @return True if the given user shares at least one channel with this user
	 */
	bool SharesChannelWith(User *other);

	/** Change the displayed host of a user.
	 * ALWAYS use this function, rather than writing User::dhost directly,
	 * as this triggers module events allowing the change to be syncronized to
	 * remote servers. This will also emulate a QUIT and rejoin (where configured)
	 * before setting their host field.
	 * @param host The new hostname to set
	 * @return True if the change succeeded, false if it didn't
	 */
	bool ChangeDisplayedHost(const char* host);

	/** Change the ident (username) of a user.
	 * ALWAYS use this function, rather than writing User::ident directly,
	 * as this correctly causes the user to seem to quit (where configured)
	 * before setting their ident field.
	 * @param host The new ident to set
	 * @return True if the change succeeded, false if it didn't
	 */
	bool ChangeIdent(const char* newident);

	/** Change a users realname field.
	 * ALWAYS use this function, rather than writing User::fullname directly,
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
	void SendAll(const char* command, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Compile a channel list for this user, and send it to the user 'source'
	 * Used internally by WHOIS
	 * @param The user to send the channel list to if it is not too long
	 * @return This user's channel list
	 */
	std::string ChannelList(User* source);

	/** Split the channel list in cl which came from dest, and spool it to this user
	 * Used internally by WHOIS
	 * @param dest The user the original channel list came from
	 * @param cl The  channel list as a string obtained from User::ChannelList()
	 */
	void SplitChanList(User* dest, const std::string &cl);

	/** Remove this user from all channels they are on, and delete any that are now empty.
	 * This is used by QUIT, and will not send part messages!
	 */
	void PurgeEmptyChannels();

	/** Get the connect class which this user belongs to.
	 * @return A pointer to this user's connect class
	 */
	ConnectClass *GetClass();

	/** Set the connect class to which this user belongs to.
	 * @param explicit_name Set this string to tie the user to a specific class name. Otherwise, the class is fitted by checking <connect> tags from the configuration file.
	 * @return A reference to this user's current connect class.
	 */
	ConnectClass *SetClass(const std::string &explicit_name = "");

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
	const std::string& GetOperQuit();

	/** Increases a user's command penalty by a set amount.
	 */
	void IncreasePenalty(int increase);

	/** Decreases a user's command penalty by a set amount.
	 */
	void DecreasePenalty(int decrease);

	/** Handle socket event.
	 * From EventHandler class.
	 * @param et Event type
	 * @param errornum Error number for EVENT_ERROR events
	 */
	void HandleEvent(EventType et, int errornum = 0);

	/** Default destructor
	 */
	virtual ~User();
};

/** Derived from Resolver, and performs user forward/reverse lookups.
 */
class CoreExport UserResolver : public Resolver
{
 private:
	/** User this class is 'attached' to.
	 */
	User* bound_user;
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
	UserResolver(InspIRCd* Instance, User* user, std::string to_resolve, QueryType qt, bool &cache);

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

/* Configuration callbacks */
//class ServerConfig;

#endif
