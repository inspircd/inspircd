/*


*/

#include "inspircd_config.h" 
#include "channels.h"
#include "connection.h"

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
class Invited : public classbase
{
 public:
	char channel[CHANMAX];
};


/** Holds information relevent to &lt;connect allow&gt; and &lt;connect deny&gt; tags in the config file.
 */
class ConnectClass : public classbase
{
 public:
	int type;
	int registration_timeout;
	char host[MAXBUF];
	char pass[MAXBUF];
	
	ConnectClass()
	{
		registration_timeout = 0;
		strcpy(host,"");
		strcpy(pass,"");
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
	 */
	char ident[64];

	/** The host displayed to non-opers (used for cloaking etc).
	 * This usually matches the value of userrec::host.
	 */
	char dhost[256];
	
	/** The users full name.
	 */
	char fullname[128];
	
	/** The user's mode string.
	 * This may contain any of the following RFC characters: o, w, s, i
	 * Your module may define other mode characters as it sees fit.
	 */
	char modes[MAXBUF];
	
	ucrec chans[MAXCHANS];
	
	/** The server the user is connected to.
	 */
	char server[256];
	
	/** The user's away message.
	 * If this string is empty, the user is not marked as away.
	 */
	char awaymsg[512];
	
	/** Stores the result of the last GetFullHost or GetRealHost call.
	 * You may use this to increase the speed of use of this class.
	 */
	char result[256];
	
	unsigned long timeout;

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
