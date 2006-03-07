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

#include "inspircd_config.h" 
#include "channels.h"
#include "inspstring.h"
#include "connection.h"
#include <string>
#ifdef THREADED_DNS
#include <pthread.h>
#endif
 
#ifndef __USERS_H__ 
#define __USERS_H__ 

#include "hashcomp.h"
#include "cull_list.h"
 
#define STATUS_OP	4
#define STATUS_HOP	2
#define STATUS_VOICE	1
#define STATUS_NORMAL	0

#define CC_ALLOW	0
#define CC_DENY		1

template<typename T> string ConvToStr(const T &in);

/** Holds a channel name to which a user has been invited.
 */
class Invited : public classbase
{
 public:
	 irc::string channel;
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
	char dhost[64];
	
	/** The users full name.
	 */
	char fullname[MAXGECOS+1];
	
	/** The user's mode string.
	 * This may contain any of the following RFC characters: o, w, s, i
	 * Your module may define other mode characters as it sees fit.
	 * it is limited to length 54, as there can only be a maximum of 52
	 * user modes (26 upper, 26 lower case) a null terminating char, and
	 * an optional + character.
	 */
	char modes[54];
	
	std::vector<ucrec> chans;
	
	/** The server the user is connected to.
	 */
	char* server;
	
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
	in_addr ip4;

	/* Write error string
	 */
	std::string WriteError;

	/** Maximum size this user's sendq can become
	 */
	long sendqmax;

	/** Maximum size this user's recvq can become
	 */
	long recvqmax;

	userrec();
	
	/** Returns the full displayed host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident&at;host form.
	 */
	virtual char* GetFullHost();
	
	/** Returns the full real host of the user
	 * This member function returns the hostname of the user as seen by other users
	 * on the server, in nick!ident&at;host form. If any form of hostname cloaking is in operation,
	 * e.g. through a module, then this method will ignore it and return the true hostname.
	 */
	virtual char* GetFullRealHost();
	
	/** Returns true if a user is invited to a channel.
	 */
	virtual bool IsInvited(irc::string &channel);
	
	/** Adds a channel to a users invite list (invites them to a channel)
	 */
	virtual void InviteTo(irc::string &channel);
	
	/** Removes a channel from a users invite list.
	 * This member function is called on successfully joining an invite only channel
	 * to which the user has previously been invited, to clear the invitation.
	 */
	virtual void RemoveInvite(irc::string &channel);
	
	/** Returns true or false for if a user can execute a privilaged oper command.
	 * This is done by looking up their oper type from userrec::oper, then referencing
	 * this to their oper classes and checking the commands they can execute.
	 */
	bool HasPermission(std::string &command);

	/** Calls read() to read some data for this user using their fd.
	 */
	int ReadData(void* buffer, size_t size);

	/** This method adds data to the buffer of the user.
	 * The buffer can grow to any size within limits of the available memory,
	 * managed by the size of a std::string, however if any individual line in
	 * the buffer grows over 600 bytes in length (which is 88 chars over the
	 * RFC-specified limit per line) then the method will return false and the
	 * text will not be inserted.
	 */
	bool AddBuffer(std::string a);

	/** This method returns true if the buffer contains at least one carriage return
	 * character (e.g. one complete line may be read)
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
	 */
	std::string GetBuffer();

	/** Sets the write error for a connection. This is done because the actual disconnect
	 * of a client may occur at an inopportune time such as half way through /LIST output.
	 * The WriteErrors of clients are checked at a more ideal time (in the mainloop) and
	 * errored clients purged.
	 */
	void SetWriteError(std::string error);

	/** Returns the write error which last occured on this connection or an empty string
	 * if none occured.
	 */
	std::string GetWriteError();

	/** Adds to the user's write buffer.
	 * You may add any amount of text up to this users sendq value, if you exceed the
	 * sendq value, SetWriteError() will be called to set the users error string to
	 * "SendQ exceeded", and further buffer adds will be dropped.
	 */
	void AddWriteBuf(std::string data);

	/** Flushes as much of the user's buffer to the file descriptor as possible.
	 * This function may not always flush the entire buffer, rather instead as much of it
	 * as it possibly can. If the send() call fails to send the entire buffer, the buffer
	 * position is advanced forwards and the rest of the data sent at the next call to
	 * this method.
	 */
	void FlushWriteBuf();

	/** Returns the list of channels this user has been invited to but has not yet joined.
	 */
	InvitedList* GetInviteList();

	void MakeHost(char* nhost);

	char* MakeWildHost();

	/** Shuts down and closes the user's socket
	 */
	void CloseSocket();

	virtual ~userrec();

#ifdef THREADED_DNS
	pthread_t dnsthread;
#endif
};

/** A lightweight userrec used by WHOWAS
 */
class WhoWasUser
{
 public:
	char nick[NICKMAX];
	char ident[IDENTMAX+1];
	char dhost[64];
	char host[64];
	char fullname[MAXGECOS+1];
	char server[256];
	time_t signon;
};

void AddOper(userrec* user);
void DeleteOper(userrec* user);
void kill_link(userrec *user,const char* r);
void kill_link_silent(userrec *user,const char* r);
void AddWhoWas(userrec* u);
void AddClient(int socket, int port, bool iscached, in_addr ip4);
void FullConnectUser(userrec* user, CullList* Goners);
userrec* ReHashNick(char* Old, char* New);
void force_nickchange(userrec* user,const char* newnick);
void ReadClassesAndTypes();

#endif
