/*

$Log$
Revision 1.1  2003/01/23 19:45:58  brain
Initial revision

Revision 1.7  2003/01/22 00:44:26  brain
Added documentation comments

Revision 1.6  2003/01/21 21:11:17  brain
Added documentation

Revision 1.5  2003/01/16 20:11:55  brain
fixed some ugly pointer bugs (thanks dblack and a|KK|y!)

Revision 1.4  2003/01/15 22:47:44  brain
Changed user and channel structs to classes (finally)

   
*/

#include "inspircd_config.h"
#include <time.h>
#include <vector>

#ifndef __CHANNELS_H__
#define __CHANNELS_H__

/** Holds an entry for a ban list, exemption list, or invite list.
 * This class contains a single element in a channel list, such as a banlist.
 */
class HostItem
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
typedef vector<BanItem> 	BanList;

/** Holds a complete exempt list
 */
typedef vector<ExemptItem>	ExemptList;

/** Holds a complete invite list
 */
typedef vector<InviteItem>	InviteList;

/** Holds all relevent information for a channel.
 * This class represents a channel, and contains its name, modes, time created, topic, topic set time,
 * etc, and an instance of the BanList type.
 */
class chanrec
{
 public:
	/** The channels name.
	 */
	char name[CHANMAX]; /* channel name */
	/** Custom modes for the channel.
	 * Plugins may use this field in any way they see fit.
	 */
	char custom_modes[MAXMODES];     /* modes handled by modules */
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
	long limit;
	
	/** Contains the channel key.
	 * If this value is an empty string, there is no channel key in place.
	 */
	char key[32];
	
	/** Nonzero if the mode +t is set.
	 */
	short int topiclock;
	
	/** Nonzero if the mode +n is set.
	 */
	short int noexternal;
	
	/** Nonzero if the mode +i is set.
	 */
	short int inviteonly;
	
	/** Nonzero if the mode +m is set.
	 */
	short int moderated;
	
	/** Nonzero if the mode +s is set.
	 * This value cannot be set at the same time as chanrec::c_private
	 */
	short int secret;
	
	/** Nonzero if the mode +p is set.
	 * This value cannot be set at the same time as chanrec::secret
	 */
	short int c_private;
	
	/** The list of all bans set on the channel.
	 */
	BanList bans;

	/** Creates a channel record and initialises it with default values
	 */
	chanrec()
	{
		strcpy(name,"");
		strcpy(custom_modes,"");
		strcpy(topic,"");
		strcpy(setby,"");
		strcpy(key,"");
		created = topicset = limit = 0;
		topiclock = noexternal = inviteonly = moderated = secret = c_private = false;
	}

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
class ucrec
{
 public:
	/** Contains a bitmask of the UCMODE_OP ... UCMODE_FOUNDER values.
	 * If this value is zero, the user has no privilages upon the channel.
	 */
	long uc_modes;
	
	/** Points to the channel record where the given modes apply.
	 * If the record is not in use, this value will be NULL.
	 */
	chanrec *channel;

	ucrec() { /* stub */ }
	virtual ~ucrec() { /* stub */ }
};

#endif

