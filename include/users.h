/*

$Log$
Revision 1.1  2003/01/23 19:45:58  brain
Initial revision

Revision 1.9  2003/01/22 00:44:26  brain
Added documentation comments

Revision 1.8  2003/01/21 21:11:17  brain
Added documentation

Revision 1.7  2003/01/17 13:21:38  brain
Added CONNECT ALLOW and CONNECT DENY config tags
Added PASS command

Revision 1.6  2003/01/17 10:37:55  brain
Added /INVITE command and relevent structures

Revision 1.5  2003/01/16 20:11:56  brain
fixed some ugly pointer bugs (thanks dblack and a|KK|y!)

Revision 1.4  2003/01/15 22:47:44  brain
Changed user and channel structs to classes (finally)

Revision 1.3  2003/01/14 21:14:30  brain
added /ISON command (for mIRC etc basic notify)


*/

#include "inspircd_config.h" 
#include "channels.h"

#include <string>
 
#ifndef __USERS_H__ 
#define __USERS_H__ 
 
#define STATUS_OP	4
#define STATUS_HOP	2
#define STATUS_VOICE	1
#define STATUS_NORMAL	0

#define CC_ALLOW	0
#define CC_DENY		1

/** Holds a channel name to which a user has been invited.
 */
class Invited
{
 public:
	char channel[CHANMAX];
};


/** Holds information relevent to &lt;connect allow&gt; and &lt;connect deny&gt; tags in the config file.
 */
class ConnectClass
{
 public:
	int type;
	char host[MAXBUF];
	char pass[MAXBUF];
};

/** Holds a complete list of all channels to which a user has been invited and has not yet joined.
 */
typedef vector<Invited> InvitedList;



/** Holds a complete list of all allow and deny tags from the configuration file (connection classes)
 */
typedef vector<ConnectClass> ClassVector;

/** Holds all information about a user
 * This class stores all information about a user connected to the irc server. Everything about a
 * connection is stored here primarily, from the user's socket ID (file descriptor) through to the
 * user's nickname and hostname. Use the Find method of the server class to locate a specific user
 * by nickname.
 */
class userrec
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
	
	/** The users ip address in network order.
	 */
	unsigned long ip;

	/** The users ident reply.
	 */
	char ident[64];

	/** The users hostname, or ip address in string form.
	 */
	char host[256];
	
	/** The host displayed to non-opers (used for cloaking etc).
	 * This usually matches the value of userrec::host.
	 */
	char dhost[256];
	
	/** The users full name.
	 */
	char fullname[128];
	
	/** The users file descriptor.
	 * If this is zero, the socket has been closed and the core has not yet
	 * realised and removed the record from memory.
	 */
	int fd;
	
	/** The user's mode string.
	 * This may contain any of the following RFC characters: o, w, s, i
	 * Your module may define other mode characters as it sees fit.
	 */
	char modes[32];
	
	/** The users input buffer.
	 * Used by the C recv() function.
	 */
	char inbuf[MAXBUF];
	
	/** The last time the user was pinged by the core.
	 * When this value is more than 120 seconds difference from 'time(NULL)', a ping is sent
	 * to the client. If the user has an outstanding PING request the next time this
	 * event occurs after 4 total minutes, they are disconnected.
	 */
	time_t lastping;
	
	/** The users signon time.
	 */
	time_t signon;
	
	/** The time the user last sent a message.
	 * See also userrec::lastping and userrec::signon
	 */
	time_t idle_lastmsg;
	
	/** True if the user replied to their last ping.
	 * If this is true, the user can be sent another ping at the specified time, otherwise
	 * they will be discnnected. See also userrec::lastping
	 */
	time_t nping;
	
	/** Bit 1 is set if the user sent a NICK command, bit 2 is set if the user sent a USER command.
	 * If both bits are set then the connection is awaiting MOTD. Sending of MOTD sets bit 3, and
	 * makes the value of userrec::registered == 7, showing a fully established client session.
	 */
	int registered;
	
	/** A list of the channels the user is currently on.
	 * If any of these values are NULL, the record is not in use and may be associated with
	 * a channel by the JOIN command. see RFC 1459.
	 */
	ucrec chans[MAXCHANS];
	
	/** The server the user is connected to.
	 */
	char server[256];
	
	/** The user's away message.
	 * If this string is empty, the user is not marked as away.
	 */
	char awaymsg[512];
	
	/** The port that the user connected to.
	 */
	int port;
	
	/** Stores the number of incoming bytes from the connection.
	 * Used by /STATS
	 */
	long bytes_in;
	
	/** Stores the number of outgoing bytes to the connection.
	 * Used by /STATS
	 */
	long bytes_out;
	
	/** Stores the number of incoming commands from the connection.
	 * Used by /STATS
	 */
	long cmds_in;
	
	/** Stores the number of outgoing commands to the connection.
	 * Used by /STATS
	 */
	long cmds_out;
	
	/** Stores the result of the last GetFullHost or GetRealHost call.
	 * You may use this to increase the speed of use of this class.
	 */
	char result[256];
	
	/** True if a correct password has been given using PASS command.
	 * If the user is a member of a connection class that does not require a password,
	 * the value stored here is of no use.
	 */
	bool haspassed;

	userrec();
	
	virtual ~userrec() {  }
	
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
	virtual bool IsInvited(char* channel);
	
	/** Adds a channel to a users invite list (invites them to a channel)
	 */
	virtual void InviteTo(char* channel);
	
	/** Removes a channel from a users invite list.
	 * This member function is called on successfully joining an invite only channel
	 * to which the user has previously been invited, to clear the invitation.
	 */
	virtual void RemoveInvite(char* channel);
	
};


#endif
