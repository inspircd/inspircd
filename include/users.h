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


#pragma once

#include "socket.h"
#include "inspsocket.h"
#include "mode.h"
#include "membership.h"

/** connect class types
 */
enum ClassTypes {
	/** connect:allow */
	CC_ALLOW = 0,
	/** connect:deny */
	CC_DENY  = 1,
	/** named connect block (for opers, etc) */
	CC_NAMED = 2
};

/** Registration state of a user, e.g.
 * have they sent USER, NICK, PASS yet?
 */
enum RegistrationState {

#ifndef _WIN32   // Burlex: This is already defined in win32, luckily it is still 0.
	REG_NONE = 0,		/* Has sent nothing */
#endif

	REG_USER = 1,		/* Has sent USER */
	REG_NICK = 2,		/* Has sent NICK */
	REG_NICKUSER = 3, 	/* Bitwise combination of REG_NICK and REG_USER */
	REG_ALL = 7	  	/* REG_NICKUSER plus next bit along */
};

enum UserType {
	USERTYPE_LOCAL = 1,
	USERTYPE_REMOTE = 2,
	USERTYPE_SERVER = 3
};

/** Holds information relevent to &lt;connect allow&gt; and &lt;connect deny&gt; tags in the config file.
 */
struct CoreExport ConnectClass : public refcountbase
{
	reference<ConfigTag> config;
	/** Type of line, either CC_ALLOW or CC_DENY
	 */
	char type;

	/** True if this class uses fake lag to manage flood, false if it kills */
	bool fakelag;

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

	/** Maximum size of sendq for users in this class (bytes)
	 * Users cannot send commands if they go over this limit
	 */
	unsigned long softsendqmax;

	/** Maximum size of sendq for users in this class (bytes)
	 * Users are killed if they go over this limit
	 */
	unsigned long hardsendqmax;

	/** Maximum size of recvq for users in this class (bytes)
	 */
	unsigned long recvqmax;

	/** Seconds worth of penalty before penalty system activates
	 */
	unsigned int penaltythreshold;

	/** Maximum rate of commands (units: millicommands per second) */
	unsigned int commandrate;

	/** Local max when connecting by this connection class
	 */
	unsigned long maxlocal;

	/** Global max when connecting by this connection class
	 */
	unsigned long maxglobal;

	/** True if max connections for this class is hit and a warning is wanted
	 */
	bool maxconnwarn;

	/** Max channels for this class
	 */
	unsigned int maxchans;

	/** How many users may be in this connect class before they are refused?
	 * (0 = no limit = default)
	 */
	unsigned long limit;

	/** If set to true, no user DNS lookups are to be performed
	 */
	bool resolvehostnames;

	/** Create a new connect class with no settings.
	 */
	ConnectClass(ConfigTag* tag, char type, const std::string& mask);
	/** Create a new connect class with inherited settings.
	 */
	ConnectClass(ConfigTag* tag, char type, const std::string& mask, const ConnectClass& parent);

	/** Update the settings in this block to match the given block */
	void Update(const ConnectClass* newSettings);

	const std::string& GetName() { return name; }
	const std::string& GetHost() { return host; }

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

	/** Returns the maximum sendq value (soft limit)
	 * Note that this is in addition to internal OS buffers
	 */
	unsigned long GetSendqSoftMax()
	{
		return (softsendqmax ? softsendqmax : 4096);
	}

	/** Returns the maximum sendq value (hard limit)
	 */
	unsigned long GetSendqHardMax()
	{
		return (hardsendqmax ? hardsendqmax : 0x100000);
	}

	/** Returns the maximum recvq value
	 */
	unsigned long GetRecvqMax()
	{
		return (recvqmax ? recvqmax : 4096);
	}

	/** Returns the penalty threshold value
	 */
	unsigned int GetPenaltyThreshold()
	{
		return penaltythreshold ? penaltythreshold : (fakelag ? 10 : 20);
	}

	unsigned int GetCommandRate()
	{
		return commandrate ? commandrate : 1000;
	}

	/** Return the maximum number of local sessions
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

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname.
 */
class CoreExport User : public Extensible
{
 private:
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

	/** The user's mode list.
	 * Much love to the STL for giving us an easy to use bitset, saving us RAM.
	 * if (modes[modeletter-65]) is set, then the mode is
	 * set, for example, to work out if mode +s is set, we check the field
	 * User::modes['s'-65] != 0.
	 */
	std::bitset<64> modes;

 public:

	/** Hostname of connection.
	 * This should be valid as per RFC1035.
	 */
	std::string host;

	/** Time that the object was instantiated (used for TS calculation etc)
	*/
	time_t age;

	/** Time the connection was created, set in the constructor. This
	 * may be different from the time the user's classbase object was
	 * created.
	 */
	time_t signon;

	/** Client address that the user is connected from.
	 * Do not modify this value directly, use SetClientIP() to change it.
	 * Port is not valid for remote users.
	 */
	irc::sockets::sockaddrs client_sa;

	/** The users nickname.
	 * An invalid nickname indicates an unregistered connection prior to the NICK command.
	 * Use InspIRCd::IsNick() to validate nicknames.
	 */
	std::string nick;

	/** The user's unique identifier.
	 * This is the unique identifier which the user has across the network.
	 */
	const std::string uuid;

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

	/** What snomasks are set on this user.
	 * This functions the same as the above modes.
	 */
	std::bitset<64> snomasks;

	/** Channels this user is on
	 */
	UserChanList chans;

	/** The server the user is connected to.
	 */
	const std::string server;

	/** The user's away message.
	 * If this string is empty, the user is not marked as away.
	 */
	std::string awaymsg;

	/** Time the user last went away.
	 * This is ONLY RELIABLE if user IsAway()!
	 */
	time_t awaytime;

	/** The oper type they logged in as, if they are an oper.
	 */
	reference<OperInfo> oper;

	/** Used by User to indicate the registration status of the connection
	 * It is a bitfield of the REG_NICK, REG_USER and REG_ALL bits to indicate
	 * the connection state.
	 */
	unsigned int registered:3;

	/** Whether or not to send an snotice about this user's quitting
	 */
	unsigned int quietquit:1;

	/** If this is set to true, then all socket operations for the user
	 * are dropped into the bit-bucket.
	 * This value is set by QuitUser, and is not needed separately from that call.
	 * Please note that setting this value alone will NOT cause the user to quit.
	 */
	unsigned int quitting:1;

	/** What type of user is this? */
	const unsigned int usertype:2;

	/** Get client IP string from sockaddr, using static internal buffer
	 * @return The IP string
	 */
	const std::string& GetIPString();

	/** Get CIDR mask, using default range, for this user
	 */
	irc::sockets::cidr_mask GetCIDRMask();

	/** Sets the client IP for this user
	 * @return true if the conversion was successful
	 */
	virtual bool SetClientIP(const char* sip, bool recheck_eline = true);

	virtual void SetClientIP(const irc::sockets::sockaddrs& sa, bool recheck_eline = true);

	/** Constructor
	 * @throw CoreException if the UID allocated to the user already exists
	 */
	User(const std::string &uid, const std::string& srv, int objtype);

	/** Returns the full displayed host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident\@host form.
	 * @return The full masked host of the user
	 */
	virtual const std::string& GetFullHost();

	/** Returns the full real host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident\@host form. If any form of hostname cloaking is in operation,
	 * e.g. through a module, then this method will ignore it and return the true hostname.
	 * @return The full real host of the user
	 */
	virtual const std::string& GetFullRealHost();

	/** This clears any cached results that are used for GetFullRealHost() etc.
	 * The results of these calls are cached as generating them can be generally expensive.
	 */
	void InvalidateCache();

	/** Returns whether this user is currently away or not. If true,
	 * further information can be found in User::awaymsg and User::awaytime
	 * @return True if the user is away, false otherwise
	 */
	bool IsAway() const { return (!awaymsg.empty()); }

	/** Returns whether this user is an oper or not. If true,
	 * oper information can be obtained from User::oper
	 * @return True if the user is an oper, false otherwise
	 */
	bool IsOper() const { return oper; }

	/** Returns true if a notice mask is set
	 * @param sm A notice mask character to check
	 * @return True if the notice mask is set
	 */
	bool IsNoticeMaskSet(unsigned char sm);

	/** Create a displayable mode string for this users umodes
	 * @param showparameters The mode string
	 */
	const char* FormatModes(bool showparameters = false);

	/** Returns true if a specific mode is set
	 * @param m The user mode
	 * @return True if the mode is set
	 */
	bool IsModeSet(unsigned char m);
	bool IsModeSet(ModeHandler* mh);
	bool IsModeSet(ModeHandler& mh) { return IsModeSet(&mh); }
	bool IsModeSet(UserModeReference& moderef);

	/** Set a specific usermode to on or off
	 * @param m The user mode
	 * @param value On or off setting of the mode
	 */
	void SetMode(ModeHandler* mh, bool value);
	void SetMode(ModeHandler& mh, bool value) { SetMode(&mh, value); }

	/** Returns true or false for if a user can execute a privilaged oper command.
	 * This is done by looking up their oper type from User::oper, then referencing
	 * this to their oper classes and checking the commands they can execute.
	 * @param command A command (should be all CAPS)
	 * @return True if this user can execute the command
	 */
	virtual bool HasPermission(const std::string &command);

	/** Returns true if a user has a given permission.
	 * This is used to check whether or not users may perform certain actions which admins may not wish to give to
	 * all operators, yet are not commands. An example might be oper override, mass messaging (/notice $*), etc.
	 *
	 * @param privstr The priv to chec, e.g. "users/override/topic". These are loaded free-form from the config file.
	 * @param noisy If set to true, the user is notified that they do not have the specified permission where applicable. If false, no notification is sent.
	 * @return True if this user has the permission in question.
	 */
	virtual bool HasPrivPermission(const std::string &privstr, bool noisy = false);

	/** Returns true or false if a user can set a privileged user or channel mode.
	 * This is done by looking up their oper type from User::oper, then referencing
	 * this to their oper classes, and checking the modes they can set.
	 * @param mode The mode the check
	 * @param type ModeType (MODETYPE_CHANNEL or MODETYPE_USER).
	 * @return True if the user can set or unset this mode.
	 */
	virtual bool HasModePermission(unsigned char mode, ModeType type);

	/** Creates a usermask with real host.
	 * Takes a buffer to use and fills the given buffer with the hostmask in the format user\@host
	 * @return the usermask in the format user\@host
	 */
	const std::string& MakeHost();

	/** Creates a usermask with real ip.
	 * Takes a buffer to use and fills the given buffer with the ipmask in the format user\@ip
	 * @return the usermask in the format user\@ip
	 */
	const std::string& MakeHostIP();

	/** Oper up the user using the given opertype.
	 * This will also give the +o usermode.
	 */
	void Oper(OperInfo* info);

	/** Force a nickname change.
	 * If the nickname change fails (for example, because the nick in question
	 * already exists) this function will return false, and you must then either
	 * output an error message, or quit the user for nickname collision.
	 * @param newnick The nickname to change to
	 * @return True if the nickchange was successful.
	 */
	bool ForceNickChange(const std::string& newnick) { return ChangeNick(newnick, true); }

	/** Oper down.
	 * This will clear the +o usermode and unset the user's oper type
	 */
	void UnOper();

	/** Write text to this user, appending CR/LF. Works on local users only.
	 * @param text A std::string to send to the user
	 */
	virtual void Write(const std::string &text);

	/** Write text to this user, appending CR/LF.
	 * Works on local users only.
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	virtual void Write(const char *text, ...) CUSTOM_PRINTF(2, 3);

	/** Write text to this user, appending CR/LF and prepending :server.name
	 * Works on local users only.
	 * @param text A std::string to send to the user
	 */
	void WriteServ(const std::string& text);

	/** Write text to this user, appending CR/LF and prepending :server.name
	 * Works on local users only.
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteServ(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Sends a server notice to this user.
	 * @param text The contents of the message to send.
	 */
	void WriteNotice(const std::string& text);

	void WriteNumeric(unsigned int numeric, const char* text, ...) CUSTOM_PRINTF(3, 4);

	void WriteNumeric(unsigned int numeric, const std::string &text);

	/** Write text to this user, appending CR/LF and prepending :nick!user\@host of the user provided in the first parameter.
	 * @param user The user to prepend the :nick!user\@host of
	 * @param text A std::string to send to the user
	 */
	void WriteFrom(User *user, const std::string &text);

	/** Write text to this user, appending CR/LF and prepending :nick!user\@host of the user provided in the first parameter.
	 * @param user The user to prepend the :nick!user\@host of
	 * @param text The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteFrom(User *user, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Write text to the user provided in the first parameter, appending CR/LF, and prepending THIS user's :nick!user\@host.
	 * @param dest The user to route the message to
	 * @param data A std::string to send to the user
	 */
	void WriteTo(User *dest, const std::string &data);

	/** Write text to the user provided in the first parameter, appending CR/LF, and prepending THIS user's :nick!user\@host.
	 * @param dest The user to route the message to
	 * @param data The format string for text to send to the user
	 * @param ... POD-type format arguments
	 */
	void WriteTo(User *dest, const char *data, ...) CUSTOM_PRINTF(3, 4);

	/** Write to all users that can see this user (including this user in the list if include_self is true), appending CR/LF
	 * @param line A std::string to send to the users
	 * @param include_self Should the message be sent back to the author?
	 */
	void WriteCommonRaw(const std::string &line, bool include_self = true);

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

	/** Write a quit message to all common users, as in User::WriteCommonExcept but with a specific
	 * quit message for opers only.
	 * @param normal_text Normal user quit message
	 * @param oper_text Oper only quit message
	 */
	void WriteCommonQuit(const std::string &normal_text, const std::string &oper_text);

	/** Dump text to a user target, splitting it appropriately to fit
	 * @param linePrefix text to prefix each complete line with
	 * @param textStream the text to send to the user
	 */
	void SendText(const std::string& linePrefix, std::stringstream& textStream);

	/** Write to the user, routing the line if the user is remote.
	 */
	virtual void SendText(const std::string& line) = 0;

	/** Write to the user, routing the line if the user is remote.
	 */
	void SendText(const char* text, ...) CUSTOM_PRINTF(2, 3);

	/** Return true if the user shares at least one channel with another user
	 * @param other The other user to compare the channel list against
	 * @return True if the given user shares at least one channel with this user
	 */
	bool SharesChannelWith(User *other);

	/** Change the displayed host of a user.
	 * ALWAYS use this function, rather than writing User::dhost directly,
	 * as this triggers module events allowing the change to be syncronized to
	 * remote servers.
	 * @param host The new hostname to set
	 * @return True if the change succeeded, false if it didn't
	 * (a module vetoed the change).
	 */
	bool ChangeDisplayedHost(const std::string& host);

	/** Change the ident (username) of a user.
	 * ALWAYS use this function, rather than writing User::ident directly,
	 * as this triggers module events allowing the change to be syncronized to
	 * remote servers.
	 * @param newident The new ident to set
	 * @return True if the change succeeded, false if it didn't
	 */
	bool ChangeIdent(const std::string& newident);

	/** Change a users realname field.
	 * ALWAYS use this function, rather than writing User::fullname directly,
	 * as this triggers module events allowing the change to be syncronized to
	 * remote servers.
	 * @param gecos The user's new realname
	 * @return True if the change succeeded, false if otherwise
	 */
	bool ChangeName(const std::string& gecos);

	/** Change a user's nick
	 * @param newnick The new nick
	 * @param force True if the change is being forced (should not be blocked by modes like +N)
	 * @return True if the change succeeded
	 */
	bool ChangeNick(const std::string& newnick, bool force = false);

	/** Send a command to all local users from this user
	 * The command given must be able to send text with the
	 * first parameter as a servermask (e.g. $*), so basically
	 * you should use PRIVMSG or NOTICE.
	 * @param command the command to send
	 * @param text The text format string to send
	 * @param ... Format arguments
	 */
	void SendAll(const char* command, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Remove this user from all channels they are on, and delete any that are now empty.
	 * This is used by QUIT, and will not send part messages!
	 */
	void PurgeEmptyChannels();

	/** Default destructor
	 */
	virtual ~User();
	virtual CullResult cull();
};

class CoreExport UserIOHandler : public StreamSocket
{
 public:
	LocalUser* const user;
	UserIOHandler(LocalUser* me) : user(me) {}
	void OnDataReady();
	void OnError(BufferedSocketError error);

	/** Adds to the user's write buffer.
	 * You may add any amount of text up to this users sendq value, if you exceed the
	 * sendq value, the user will be removed, and further buffer adds will be dropped.
	 * @param data The data to add to the write buffer
	 */
	void AddWriteBuf(const std::string &data);
};

typedef unsigned int already_sent_t;

class CoreExport LocalUser : public User, public InviteBase
{
 public:
	LocalUser(int fd, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server);
	CullResult cull();

	UserIOHandler eh;

	/** Position in UserManager::local_users
	 */
	LocalUserList::iterator localuseriter;

	/** Stats counter for bytes inbound
	 */
	unsigned int bytes_in;

	/** Stats counter for bytes outbound
	 */
	unsigned int bytes_out;

	/** Stats counter for commands inbound
	 */
	unsigned int cmds_in;

	/** Stats counter for commands outbound
	 */
	unsigned int cmds_out;

	/** Password specified by the user when they registered (if any).
	 * This is stored even if the \<connect> block doesnt need a password, so that
	 * modules may check it.
	 */
	std::string password;

	/** Contains a pointer to the connect class a user is on from
	 */
	reference<ConnectClass> MyClass;

	/** Get the connect class which this user belongs to.
	 * @return A pointer to this user's connect class.
	 */
	ConnectClass* GetClass() const { return MyClass; }

	/** Call this method to find the matching \<connect> for a user, and to check them against it.
	 */
	void CheckClass(bool clone_count = true);

	/** Server address and port that this user is connected to.
	 */
	irc::sockets::sockaddrs server_sa;

	/**
	 * @return The port number of this user.
	 */
	int GetServerPort();

	/** Recursion fix: user is out of SendQ and will be quit as soon as possible.
	 * This can't be handled normally because QuitUser itself calls Write on other
	 * users, which could trigger their SendQ to overrun.
	 */
	unsigned int quitting_sendq:1;

	/** has the user responded to their previous ping?
	 */
	unsigned int lastping:1;

	/** This is true if the user matched an exception (E:Line). It is used to save time on ban checks.
	 */
	unsigned int exempt:1;

	/** Used by PING checking code
	 */
	time_t nping;

	/** Time that the connection last sent a message, used to calculate idle time
	 */
	time_t idle_lastmsg;

	/** This value contains how far into the penalty threshold the user is.
	 * This is used either to enable fake lag or for excess flood quits
	 */
	unsigned int CommandFloodPenalty;

	static already_sent_t already_sent_id;
	already_sent_t already_sent;

	/** Check if the user matches a G or K line, and disconnect them if they do.
	 * @param doZline True if ZLines should be checked (if IP has changed since initial connect)
	 * Returns true if the user matched a ban, false else.
	 */
	bool CheckLines(bool doZline = false);

	/** Use this method to fully connect a user.
	 * This will send the message of the day, check G/K/E lines, etc.
	 */
	void FullConnect();

	/** Set the connect class to which this user belongs to.
	 * @param explicit_name Set this string to tie the user to a specific class name. Otherwise, the class is fitted by checking \<connect> tags from the configuration file.
	 * @return A reference to this user's current connect class.
	 */
	void SetClass(const std::string &explicit_name = "");

	bool SetClientIP(const char* sip, bool recheck_eline = true);

	void SetClientIP(const irc::sockets::sockaddrs& sa, bool recheck_eline = true);

	void SendText(const std::string& line);
	void Write(const std::string& text);
	void Write(const char*, ...) CUSTOM_PRINTF(2, 3);

	/** Returns the list of channels this user has been invited to but has not yet joined.
	 * @return A list of channels the user is invited to
	 */
	InviteList& GetInviteList();

	/** Returns true if a user is invited to a channel.
	 * @param chan A channel to look up
	 * @return True if the user is invited to the given channel
	 */
	bool IsInvited(Channel* chan) { return (Invitation::Find(chan, this) != NULL); }

	/** Removes a channel from a users invite list.
	 * This member function is called on successfully joining an invite only channel
	 * to which the user has previously been invited, to clear the invitation.
	 * @param chan The channel to remove the invite to
	 * @return True if the user was invited to the channel and the invite was erased, false if the user wasn't invited
	 */
	bool RemoveInvite(Channel* chan);

	void RemoveExpiredInvites();

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
};

class CoreExport RemoteUser : public User
{
 public:
	RemoteUser(const std::string& uid, const std::string& srv) : User(uid, srv, USERTYPE_REMOTE)
	{
	}
	virtual void SendText(const std::string& line);
};

class CoreExport FakeUser : public User
{
 public:
	FakeUser(const std::string &uid, const std::string& srv) : User(uid, srv, USERTYPE_SERVER)
	{
		nick = srv;
	}

	virtual CullResult cull();
	virtual void SendText(const std::string& line);
	virtual const std::string& GetFullHost();
	virtual const std::string& GetFullRealHost();
};

/* Faster than dynamic_cast */
/** Is a local user */
inline LocalUser* IS_LOCAL(User* u)
{
	return u->usertype == USERTYPE_LOCAL ? static_cast<LocalUser*>(u) : NULL;
}
/** Is a remote user */
inline RemoteUser* IS_REMOTE(User* u)
{
	return u->usertype == USERTYPE_REMOTE ? static_cast<RemoteUser*>(u) : NULL;
}
/** Is a server fakeuser */
inline FakeUser* IS_SERVER(User* u)
{
	return u->usertype == USERTYPE_SERVER ? static_cast<FakeUser*>(u) : NULL;
}

inline bool User::IsModeSet(ModeHandler* mh)
{
	char m = mh->GetModeChar();
	return (modes[m-65]);
}

inline bool User::IsModeSet(UserModeReference& moderef)
{
	if (!moderef)
		return false;
	return IsModeSet(*moderef);
}

inline void User::SetMode(ModeHandler* mh, bool value)
{
	char m = mh->GetModeChar();
	modes[m-65] = value;
}
