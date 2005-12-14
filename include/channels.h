/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "base.h"
#include <time.h>
#include <vector>
#include <string>

#ifndef __CHANNELS_H__
#define __CHANNELS_H__

#define CM_TOPICLOCK 1
#define CM_NOEXTERNAL 2
#define CM_INVITEONLY 4
#define CM_MODERATED 8
#define CM_SECRET 16
#define CM_PRIVATE 32

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


/** Holds a custom parameter to a module-defined channel mode
  * e.g. for +L this would hold the channel name.
  */

class ModeParameter : public classbase
{
 public:
	char mode;
	char parameter[MAXBUF];
	char channel[CHANMAX];
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
	char custom_modes[MAXMODES];     /* modes handled by modules */

	/** User list (casted to char*'s to stop forward declaration stuff)
	 * (chicken and egg scenario!)
	 */
	std::vector<char*> internal_userlist;
	
	/** Channel topic.
	 * If this is an empty string, no channel topic is set.
	 */
	char topic[MAXBUF];
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
	 * @param castuser This should be a pointer to a userrec, casted to char*
	 *
	 * The data inserted into the reference list is a table as it is
	 * an arbitary pointer compared to other users by its memory address,
	 * as this is a very fast 32 or 64 bit integer comparison.
	 */
	void AddUser(char* castuser);

        /** Delete a user pointer to the internal reference list
	 * @param castuser This should be a pointer to a userrec, casted to char*
	 *
         * The data removed from the reference list is a table as it is
         * an arbitary pointer compared to other users by its memory address,
         * as this is a very fast 32 or 64 bit integer comparison.
         */
	void DelUser(char* castuser);

	/** Obrain the internal reference list
	 * The internal reference list contains a list of userrec*
	 * cast to char*. These are used for rapid comparison to determine
	 * channel membership for PRIVMSG, NOTICE, QUIT, PART etc.
	 * The resulting pointer to the vector should be considered
	 * readonly and only modified via AddUser and DelUser.
	 *
	 * @return This function returns a vector of userrec pointers, each of which has been casted to char* to prevent circular references
	 */
	std::vector<char*> *GetUsers();

	/** Creates a channel record and initialises it with default values
	 */
	chanrec();

	virtual ~chanrec() { /* stub */ }
};

/* used to hold a channel and a users modes on that channel, e.g. +v, +h, +o
 * needs to come AFTER struct chanrec */

#define UCMODE_OP      1
#define UCMODE_VOICE   2
#define UCMODE_HOP     4
#define UCMODE_PROTECT 8
#define UCMODE_FOUNDER 16
 
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

	ucrec() { /* stub */ }
	virtual ~ucrec() { /* stub */ }
};

chanrec* add_channel(userrec *user, const char* cn, const char* key, bool override);
chanrec* del_channel(userrec *user, const char* cname, const char* reason, bool local);
void kick_channel(userrec *src,userrec *user, chanrec *Ptr, char* reason);

#endif

