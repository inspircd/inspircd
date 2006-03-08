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

enum ChannelModes {
	CM_TOPICLOCK = 1,
	CM_NOEXTERNAL = 2,
	CM_INVITEONLY = 4,
	CM_MODERATED = 8,
	CM_SECRET = 16,
	CM_PRIVATE = 32
};

class userrec;

/** Holds an entry for a ban list, exemption list, or invite list.
 * This class contains a single element in a channel list, such as a banlist.
 */
class HostItem : public classbase
{
 public:
	time_t set_time;
	char set_by[NICKMAX];
	char data[MAXBUF];

	HostItem() { /* stub */ }
	virtual ~HostItem() { /* stub */ }
};

// banlist is inherited from HostList mainly for readability
// reasons only

/** A subclass of HostItem designed to hold channel bans (+b)
 */
class BanItem : public HostItem
{
};

// same with this...

/** A subclass of HostItem designed to hold channel exempts (+e)
 */
class ExemptItem : public HostItem
{
};

// and this...

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

class userrec;

/** A list of users on a channel
 */
typedef std::map<userrec*,userrec*> CUList;

/** A list of custom modes parameters on a channel
 */
typedef std::map<char,char*> CustomModeList;

/** Holds all relevent information for a channel.
 * This class represents a channel, and contains its name, modes, time created, topic, topic set time,
 * etc, and an instance of the BanList type.
 */
class chanrec : public Extensible
{
 public:
	/** The channels name.
	 */
	char name[CHANMAX]; /* channel name */
	/** Custom modes for the channel.
	 * Plugins may use this field in any way they see fit.
	 */
	char custom_modes[64];     /* modes handled by modules */

	/** User lists
	 * There are four user lists, one for 
	 * all the users, one for the ops, one for
	 * the halfops and another for the voices.
	 */
	CUList internal_userlist;
	CUList internal_op_userlist;
	CUList internal_halfop_userlist;
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
	
	/** Contains a bitmask of the CM_* builtin (RFC) binary mode symbols
	 */
	char binarymodes;
	
	/** The list of all bans set on the channel.
	 */
	BanList bans;
	
	/** Sets or unsets a custom mode in the channels info
	 * @param mode The mode character to set or unset
	 * @param mode_on True if you want to set the mode or false if you want to remove it
	 */
	void SetCustomMode(char mode,bool mode_on);

	/** Sets or unsets the parameters for a custom mode in a channels info
	 * @param mode The mode character to set or unset
	 * @param parameter The parameter string to associate with this mode character
	 * @param mode_on True if you want to set the mode or false if you want to remove it
	 */
	void SetCustomModeParam(char mode,char* parameter,bool mode_on);
 
	/** Returns true if a custom mode is set on a channel
	  * @param mode The mode character you wish to query
	  * @return True if the custom mode is set, false if otherwise
	  */
	bool IsCustomModeSet(char mode);

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
	void AddOppedUser(userrec* user);
	void AddHalfoppedUser(userrec* user);
	void AddVoicedUser(userrec* user);

        /** Delete a user pointer to the internal reference list
	 * @param user The user to delete
         */
	void DelUser(userrec* user);
	void DelOppedUser(userrec* user);
	void DelHalfoppedUser(userrec* user);
	void DelVoicedUser(userrec* user);

	/** Obrain the internal reference list
	 * The internal reference list contains a list of userrec*.
	 * These are used for rapid comparison to determine
	 * channel membership for PRIVMSG, NOTICE, QUIT, PART etc.
	 * The resulting pointer to the vector should be considered
	 * readonly and only modified via AddUser and DelUser.
	 *
	 * @return This function returns pointer to a map of userrec pointers (CUList*).
	 */
	CUList* GetUsers();
	CUList* GetOppedUsers();
	CUList* GetHalfoppedUsers();
	CUList* GetVoicedUsers();

	bool HasUser(userrec* user);

	/** Creates a channel record and initialises it with default values
	 */
	chanrec();

	virtual ~chanrec() { /* stub */ }
};

/** used to hold a channel and a users modes on that channel, e.g. +v, +h, +o
 * needs to come AFTER struct chanrec */
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

	ucrec() : uc_modes(0), channel(NULL) { /* stub */ }
	virtual ~ucrec() { /* stub */ }
};

chanrec* add_channel(userrec *user, const char* cn, const char* key, bool override);
chanrec* del_channel(userrec *user, const char* cname, const char* reason, bool local);
void kick_channel(userrec *src,userrec *user, chanrec *Ptr, char* reason);
void server_kick_channel(userrec* user, chanrec* Ptr, char* reason, bool triggerevents);

#endif

