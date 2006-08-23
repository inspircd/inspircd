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

#ifndef __CHANNELS_H__
#define __CHANNELS_H__

#include "inspircd_config.h"
#include "base.h"
#include <time.h>
#include <vector>
#include <string>
#include <map>

/** RFC1459 channel modes
 */
enum ChannelModes {
	CM_TOPICLOCK = 't'-65,
	CM_NOEXTERNAL = 'n'-65,
	CM_INVITEONLY = 'i'-65,
	CM_MODERATED = 'm'-65,
	CM_SECRET = 's'-65,
	CM_PRIVATE = 'p'-65,
	CM_KEY = 'k'-65,
	CM_LIMIT = 'l'-65
};

class userrec;
class chanrec;

/** Holds an entry for a ban list, exemption list, or invite list.
 * This class contains a single element in a channel list, such as a banlist.
 */
class HostItem : public classbase
{
 public:
	/** Time the item was added
	 */
	time_t set_time;
	/** Who added the item
	 */
	char set_by[NICKMAX];
	/** The actual item data
	 */
	char data[MAXBUF];

	HostItem() { /* stub */ }
	virtual ~HostItem() { /* stub */ }
};

/** A subclass of HostItem designed to hold channel bans (+b)
 */
class BanItem : public HostItem
{
};

/** A subclass of HostItem designed to hold channel exempts (+e)
 */
class ExemptItem : public HostItem
{
};

/** A subclass of HostItem designed to hold channel invites (+I)
 */
class InviteItem : public HostItem
{
};

/** Holds a complete ban list
 */
typedef std::vector<BanItem> 	BanList;

/** Holds a complete exempt list
 */
typedef std::vector<ExemptItem>	ExemptList;

/** Holds a complete invite list
 */
typedef std::vector<InviteItem>	InviteList;

/** A list of users on a channel
 */
typedef std::map<userrec*,userrec*> CUList;

/** Shorthand for CUList::iterator
 */
typedef CUList::iterator CUListIter;

/** Shorthand for CUList::const_iterator
 */
typedef CUList::const_iterator CUListConstIter;

/** A list of custom modes parameters on a channel
 */
typedef std::map<char,char*> CustomModeList;


/** used to hold a channel and a users modes on that channel, e.g. +v, +h, +o
 */
enum UserChannelModes {
	UCMODE_OP      = 1,
	UCMODE_VOICE   = 2,
	UCMODE_HOP     = 4
};

/** Holds a user's modes on a channel
 * This class associates a users privilages with a channel by creating a pointer link between
 * a userrec and chanrec class. The uc_modes member holds a bitmask of which privilages the user
 * has on the channel, such as op, voice, etc.
 */
class ucrec : public classbase
{
 public:
	/** Contains a bitmask of the UCMODE_OP ... UCMODE_FOUNDER values.
	 * If this value is zero, the user has no privilages upon the channel.
	 */
	char uc_modes;

	/** Points to the channel record where the given modes apply.
	 * If the record is not in use, this value will be NULL.
	 */
	chanrec *channel;

	/** Constructor for ucrec
	 */
	ucrec() : uc_modes(0), channel(NULL) { /* stub */ }

	/** Destructor for ucrec
	 */
	virtual ~ucrec() { /* stub */ }
};

class InspIRCd;

typedef std::pair<char, unsigned int> prefixtype;
typedef std::vector<prefixtype> pfxcontainer;
typedef std::map<userrec*, std::vector<prefixtype> > prefixlist;

/** Holds all relevent information for a channel.
 * This class represents a channel, and contains its name, modes, time created, topic, topic set time,
 * etc, and an instance of the BanList type.
 */
class chanrec : public Extensible
{
 private:

	/** Pointer to creator object
	 */
	InspIRCd* ServerInstance;

	/** Connect a chanrec to a userrec
	 */
	static chanrec* ForceChan(InspIRCd* Instance, chanrec* Ptr,ucrec *a,userrec* user, int created);

	prefixlist prefixes;

 public:
	/** The channels name.
	 */
	char name[CHANMAX];

	/** Modes for the channel.
	 * This is not a null terminated string! It is a hash where
	 * each item in it represents if a mode is set. For example
	 * for mode +A, index 0. Use modechar-65 to calculate which
	 * field to check.
	 */
	char modes[64];

	/** User lists
	 * There are four user lists, one for 
	 * all the users, one for the ops, one for
	 * the halfops and another for the voices.
	 */
	CUList internal_userlist;

	/** Opped users
	 */
	CUList internal_op_userlist;

	/** Halfopped users
	 */
	CUList internal_halfop_userlist;

	/** Voiced users
	 */
	CUList internal_voice_userlist;

	/** Parameters for custom modes
	 */
	CustomModeList custom_mode_params;

	/** Channel topic.
	 * If this is an empty string, no channel topic is set.
	 */
	char topic[MAXTOPIC];
	/** Creation time.
	 */
	time_t created;
	/** Time topic was set.
	 * If no topic was ever set, this will be equal to chanrec::created
	 */
	time_t topicset;
	/** The last user to set the topic.
	 * If this member is an empty string, no topic was ever set.
	 */
	char setby[NICKMAX];

	/** Contains the channel user limit.
	 * If this value is zero, there is no limit in place.
	 */
	short int limit;
	
	/** Contains the channel key.
	 * If this value is an empty string, there is no channel key in place.
	 */
	char key[32];

	/** The list of all bans set on the channel.
	 */
	BanList bans;
	
	/** Sets or unsets a custom mode in the channels info
	 * @param mode The mode character to set or unset
	 * @param mode_on True if you want to set the mode or false if you want to remove it
	 */
	void SetMode(char mode,bool mode_on);

	/** Sets or unsets the parameters for a custom mode in a channels info
	 * @param mode The mode character to set or unset
	 * @param parameter The parameter string to associate with this mode character
	 * @param mode_on True if you want to set the mode or false if you want to remove it
	 */
	void SetModeParam(char mode,const char* parameter,bool mode_on);
 
	/** Returns true if a mode is set on a channel
	  * @param mode The mode character you wish to query
	  * @return True if the custom mode is set, false if otherwise
	  */
	bool IsModeSet(char mode);

	/** Returns the parameter for a custom mode on a channel.
	  * @param mode The mode character you wish to query
	  *
	  * For example if "+L #foo" is set, and you pass this method
	  * 'L', it will return '#foo'. If the mode is not set on the
	  * channel, or the mode has no parameters associated with it,
	  * it will return an empty string.
	  *
	  * @return The parameter for this mode is returned, or an empty string
	  */
	std::string GetModeParameter(char mode);

	/** Obtain the channel "user counter"
	 * This returns the channel reference counter, which is initialized
	 * to 0 when the channel is created and incremented/decremented
	 * upon joins, parts quits and kicks.
	 *
	 * @return The number of users on this channel
	 */
	long GetUserCounter();

	/** Add a user pointer to the internal reference list
	 * @param user The user to add
	 *
	 * The data inserted into the reference list is a table as it is
	 * an arbitary pointer compared to other users by its memory address,
	 * as this is a very fast 32 or 64 bit integer comparison.
	 */
	void AddUser(userrec* user);

	/** Add a user pointer to the internal reference list of opped users
	 * @param user The user to add
	 */
	void AddOppedUser(userrec* user);

	/** Add a user pointer to the internal reference list of halfopped users
	 * @param user The user to add
	 */
	void AddHalfoppedUser(userrec* user);

	/** Add a user pointer to the internal reference list of voiced users
	 * @param user The user to add
	 */
	void AddVoicedUser(userrec* user);

        /** Delete a user pointer to the internal reference list
	 * @param user The user to delete
	 * @return number of users left on the channel after deletion of the user
         */
	unsigned long DelUser(userrec* user);

	/** Delete a user pointer to the internal reference list of opped users
	 * @param user The user to delete
	 */
	void DelOppedUser(userrec* user);

	/** Delete a user pointer to the internal reference list of halfopped users
	 * @param user The user to delete
	 */
	void DelHalfoppedUser(userrec* user);

	/** Delete a user pointer to the internal reference list of voiced users
	 * @param user The user to delete
	 */
	void DelVoicedUser(userrec* user);

	/** Obtain the internal reference list
	 * The internal reference list contains a list of userrec*.
	 * These are used for rapid comparison to determine
	 * channel membership for PRIVMSG, NOTICE, QUIT, PART etc.
	 * The resulting pointer to the vector should be considered
	 * readonly and only modified via AddUser and DelUser.
	 *
	 * @return This function returns pointer to a map of userrec pointers (CUList*).
	 */
	CUList* GetUsers();

	/** Obtain the internal reference list of opped users
	 * @return This function returns pointer to a map of userrec pointers (CUList*).
	 */
	CUList* GetOppedUsers();

	/** Obtain the internal reference list of halfopped users
	 * @return This function returns pointer to a map of userrec pointers (CUList*).
	 */
	CUList* GetHalfoppedUsers();

	/** Obtain the internal reference list of voiced users
	 * @return This function returns pointer to a map of userrec pointers (CUList*).
	 */
	CUList* GetVoicedUsers();

	/** Returns true if the user given is on the given channel.
	 * @param The user to look for
	 * @return True if the user is on this channel
	 */
	bool HasUser(userrec* user);

	/** Creates a channel record and initialises it with default values
	 * @throw Nothing at present.
	 */
	chanrec(InspIRCd* Instance);

	/** Make src kick user from this channel with the given reason.
	 * @param src The source of the kick
	 * @param user The user being kicked (must be on this channel)
	 * @param reason The reason for the kick
	 * @return The number of users left on the channel. If this is zero
	 * when the method returns, you MUST delete the chanrec immediately!
	 */
	long KickUser(userrec *src, userrec *user, const char* reason);

	/** Make the server kick user from this channel with the given reason.
	 * @param user The user being kicked (must be on this channel)
	 * @param reason The reason for the kick
	 * @param triggerevents True if you wish this kick to trigger module events
	 * @return The number of users left on the channel. If this is zero
	 * when the method returns, you MUST delete the chanrec immediately!
	 */
	long ServerKickUser(userrec* user, const char* reason, bool triggerevents);

	/** Part a user from this channel with the given reason.
	 * If the reason field is NULL, no reason will be sent.
	 * @param user The user who is parting (must be on this channel)
	 * @param reason The (optional) part reason
	 * @return The number of users left on the channel. If this is zero
	 * when the method returns, you MUST delete the chanrec immediately!
	 */
	long PartUser(userrec *user, const char* reason = NULL);

	/* Join a user to a channel. May be a channel that doesnt exist yet.
	 * @param user The user to join to the channel.
	 * @param cn The channel name to join to. Does not have to exist.
	 * @param key The key of the channel, if given
	 * @param override If true, override all join restrictions such as +bkil
	 * @return A pointer to the chanrec the user was joined to. A new chanrec may have
	 * been created if the channel did not exist before the user was joined to it.
	 * If the user could not be joined to a channel, the return value may be NULL.
	 */
	static chanrec* JoinUser(InspIRCd* ServerInstance, userrec *user, const char* cn, bool override, const char* key = "");

	/** Write to a channel, from a user, using va_args for text
	 * @param user User whos details to prefix the line with
	 * @param text A printf-style format string which builds the output line without prefix
	 * @param ... Zero or more POD types
	 */
	void WriteChannel(userrec* user, char* text, ...);

	/** Write to a channel, from a user, using std::string for text
	 * @param user User whos details to prefix the line with
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteChannel(userrec* user, const std::string &text);

	/** Write to a channel, from a server, using va_args for text
	 * @param ServName Server name to prefix the line with
	 * @param text A printf-style format string which builds the output line without prefi
	 * @param ... Zero or more POD type
	 */
	void WriteChannelWithServ(const char* ServName, const char* text, ...);

	/** Write to a channel, from a server, using std::string for text
	 * @param ServName Server name to prefix the line with
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteChannelWithServ(const char* ServName, const std::string &text);

	/** Write to all users on a channel except a specific user, using va_args for text
	 * @param user User whos details to prefix the line with, and to omit from receipt of the message
	 * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
	 * @param text A printf-style format string which builds the output line without prefi
	 * @param ... Zero or more POD type
	 */
	void WriteAllExceptSender(userrec* user, char status, char* text, ...);

	/** Write to all users on a channel except a specific user, using std::string for text
	 * @param user User whos details to prefix the line with, and to omit from receipt of the message
	 * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteAllExceptSender(userrec* user, char status, const std::string& text);

	/** Returns the maximum number of bans allowed to be set on this channel
	 * @return The maximum number of bans allowed
	 */
	long GetMaxBans();

	/** Return the channel's modes with parameters.
	 * @param showkey If this is set to true, the actual key is shown,
	 * otherwise it is replaced with '&lt;KEY&gt;'
	 * @return The channel mode string
	 */
	char* ChanModes(bool showkey);

	/** Spool the NAMES list for this channel to the given user
	 * @param The user to spool the NAMES list to
	 */
	void UserList(userrec *user);

	/** Get the number of invisible users on this channel
	 * @return Number of invisible users
	 */
	int CountInvisible();

	/** Get a users status on this channel
	 * @param The user to look up
	 * @return One of STATUS_OP, STATUS_HOP, STATUS_VOICE, or zero.
	 */
	int GetStatus(userrec *user);

	/** Get a users status on this channel in a bitmask
	 * @param The user to look up
	 * @return A bitmask containing zero or more of STATUS_OP, STATUS_HOP, STATUS_VOICE
	 */
	int GetStatusFlags(userrec *user);

	/** Get a users status on this channel in a string
	 * @param The user to look up
	 * @return A character array containing the string "@", "%", "+" or ""
	 */
	const char* GetStatusChar(userrec *user);

	void SetPrefix(userrec* user, char prefix, unsigned int prefix_rank, bool adding);

	/** Destructor for chanrec
	 */
	virtual ~chanrec() { /* stub */ }
};

#endif
