/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
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
#include "cull_list.h"

enum ChanStatus {
	STATUS_OP     = 4,
	STATUS_HOP    = 2,
	STATUS_VOICE  = 1,
	STATUS_NORMAL = 0
};

enum ClassTypes {
	CC_ALLOW = 0,
	CC_DENY  = 1
};

/** RFC1459 channel modes
 *  */
enum UserModes {
	UM_SERVERNOTICE = 's'-65,
	UM_WALLOPS = 'w'-65,
	UM_INVISIBLE = 'i'-65,
	UM_OPERATOR = 'o'-65,
};

enum RegistrationState {
	REG_NONE = 0,		/* Has sent nothing */
	REG_USER = 1,		/* Has sent USER */
	REG_NICK = 2,		/* Has sent NICK */
	REG_NICKUSER = 3, 	/* Bitwise combination of REG_NICK and REG_USER */
	REG_ALL = 7	  	/* REG_NICKUSER plus next bit along */
};

/** Holds a channel name to which a user has been invited.
 */
class Invited : public classbase
{
 public:
	 irc::string channel;
};



/** Derived from Resolver, and performs user forward/reverse lookups.
 */
class UserResolver : public Resolver
{
 private:
	/** User this class is 'attached' to.
	 */
	userrec* bound_user;
	int bound_fd;
	bool fwd;
 public:
	UserResolver(userrec* user, std::string to_resolve, bool forward);

	void OnLookupComplete(const std::string &result);
	void OnError(ResolverError e, const std::string &errormessage);
};


/** Holds information relevent to &lt;connect allow&gt; and &lt;connect deny&gt; tags in the config file.
 */
class ConnectClass : public classbase
{
 public:
	/** Type of line, either CC_ALLOW or CC_DENY
	 */
	char type;
	/** Max time to register the connection in seconds
	 */
	int registration_timeout;
	/** Number of lines in buffer before excess flood is triggered
	 */
	int flood;
	/** Host mask for this line
	 */
	std::string host;
	/** Number of seconds between pings for this line
	 */
	int pingtime;
	/** (Optional) Password for this line
	 */
	std::string pass;

	/** Threshold value for flood disconnect
	 */
	int threshold;

	/** Maximum size of sendq for users in this class (bytes)
	 */
	long sendqmax;

	/** Maximum size of recvq for users in this class (bytes)
	 */
	long recvqmax;

	/** Local max when connecting by this connection class
	 */
	long maxlocal;

	/** Global max when connecting by this connection class
	 */
	long maxglobal;
	
	ConnectClass() : registration_timeout(0), flood(0), host(""), pingtime(0), pass(""), threshold(0), sendqmax(0), recvqmax(0)
	{
	}
};

/** Holds a complete list of all channels to which a user has been invited and has not yet joined.
 */
typedef std::vector<Invited> InvitedList;



/** Holds a complete list of all allow and deny tags from the configuration file (connection classes)
 */
typedef std::vector<ConnectClass> ClassVector;

/** Typedef for the list of user-channel records for a user
 */
typedef std::vector<ucrec*> UserChanList;

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname. Use the Find method of the server class to locate a specific user
 * by nickname.
 */
class userrec : public connection
{
 private:

	/** A list of channels the user has a pending invite to.
	 */
	InvitedList invites;
 public:
	/** Resolvers for looking up this users hostname
	 */
	UserResolver* res_forward;

	/** Resolvers for looking up this users hostname
	 */
	UserResolver* res_reverse;

	/** Stored reverse lookup from res_forward
	 */
	std::string stored_host;

	/** Starts a DNS lookup of the user's IP.
	 * When complete, sets userrec::dns_done to true.
	 */
	void StartDNSLookup();
	
	/** The users nickname.
	 * An invalid nickname indicates an unregistered connection prior to the NICK command.
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
	
	/** The users full name.
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
	
	/** Number of seconds this user is given to send USER/NICK
	 * If they do not send their details in this time limit they
	 * will be disconnected
	 */
	unsigned int timeout;
	
	/** The oper type they logged in as, if they are an oper.
	 * This is used to check permissions in operclasses, so that
	 * we can say 'yay' or 'nay' to any commands they issue.
	 * The value of this is the value of a valid 'type name=' tag.
	 */
	char oper[NICKMAX];

        /** True when DNS lookups are completed.
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

	/** Flood counters
	 */
	int lines_in;
	time_t reset_due;
	long threshold;

	/** IPV4 ip address
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

	/** Maximum size this user's sendq can become
	 */
	long sendqmax;

	/** Maximum size this user's recvq can become
	 */
	long recvqmax;

	/** Default constructor
	 * @throw Nothing at present
	 */
	userrec();
	
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

	/** Create a displayable mode string for this users snomasks
	 * @return The notice mask character sequence
	 */
	const char* FormatNoticeMasks();

	/** Process a snomask modifier string, e.g. +abc-de
	 * @param sm A sequence of notice mask characters
	 * @return True if the notice masks were successfully applied
	 */
	bool userrec::ProcessNoticeMasks(const char *sm);

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
	virtual bool IsInvited(irc::string &channel);
	
	/** Adds a channel to a users invite list (invites them to a channel)
	 * @param channel A channel name to add
	 */
	virtual void InviteTo(irc::string &channel);
	
	/** Removes a channel from a users invite list.
	 * This member function is called on successfully joining an invite only channel
	 * to which the user has previously been invited, to clear the invitation.
	 * @param channel The channel to remove the invite to
	 */
	virtual void RemoveInvite(irc::string &channel);
	
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

	/** Creates a host.
	 * Takes a buffer to use and fills the given buffer with the host in the format nick!user@host
	 * @param Buffer to fill with host information
	 */
	void MakeHost(char* nhost);

	/** Shuts down and closes the user's socket
	 */
	void CloseSocket();

	/** Disconnect a user gracefully
	 * @param user The user to remove
	 * @param r The quit reason
	 */
	static void QuitUser(userrec *user, const std::string &r);

	/** Add the user to WHOWAS system
	 */
	void AddToWhoWas();

	/** Oper up the user using the given opertype.
	 * This will also give the +o usermode.
	 * @param opertype The oper type to oper as
	 */
	void Oper(const std::string &opertype);

	/** Use this method to fully connect a user.
	 * This will send the message of the day, check G/K/E lines, etc.
	 * @param Goners If the user is disconnected by this method call, the
	 * value of 'this' will be pushed onto this CullList. This is used by
	 * the core to connect many users in rapid succession without invalidating
	 * iterators.
	 */
	void FullConnect(CullList* Goners);

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
	 */
	static void AddClient(int socket, int port, bool iscached, insp_inaddr ip);

	/** Oper down.
	 * This will clear the +o usermode and unset the user's oper type
	 */
	void UnOper();

	/** Return the number of global clones of this user
	 */
	long GlobalCloneCount();

	/** Return the number of local clones of this user
	 */
	long LocalCloneCount();

	/** Write text to this user, appending CR/LF.
	 * @param text A std::string to send to the user
	 */
	void Write(const std::string &text);

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

	void WriteWallOps(const char* text, ...);

	void WriteWallOps(const std::string &text);

	bool SharesChannelWith(userrec *other);

	bool ChangeDisplayedHost(const char* host);

	bool ChangeName(const char* gecos);

	int CountChannels();

	void NoticeAll(char* text, ...);

	/** Default destructor
	 */
	virtual ~userrec();
};


namespace irc
{
	/** Holds whowas related functions and classes
	 */
	namespace whowas
	{

		/** Used to hold WHOWAS information
		 */
		class WhoWasGroup : public classbase
		{
		 public:
			/** Real host
			 */
			char* host;
			/** Displayed host
			 */
			char* dhost;
			/** Ident
			 */
			char* ident;
			/** Server name
			 */
			const char* server;
			/** Fullname (GECOS)
			 */
			char* gecos;
			/** Signon time
			 */
			time_t signon;
	
			/** Initialize this WhoQasFroup with a user
			 */
			WhoWasGroup(userrec* user);
			/** Destructor
			 */
			~WhoWasGroup();
		};

		/** A group of users related by nickname
		 */
		typedef std::deque<WhoWasGroup*> whowas_set;

		/** Sets of users in the whowas system
		 */
		typedef std::map<irc::string,whowas_set*> whowas_users;

		/** Called every hour by the core to remove expired entries
		 */
		void MaintainWhoWas(time_t TIME);
	};
};

/* Configuration callbacks */
class ServerConfig;
bool InitTypes(ServerConfig* conf, const char* tag);
bool InitClasses(ServerConfig* conf, const char* tag);
bool DoType(ServerConfig* conf, const char* tag, char** entries, void** values, int* types);
bool DoClass(ServerConfig* conf, const char* tag, char** entries, void** values, int* types);
bool DoneClassesAndTypes(ServerConfig* conf, const char* tag);

#endif
