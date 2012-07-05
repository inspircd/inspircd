/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003-2007 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef CHANNELS_H
#define CHANNELS_H

#include "membership.h"
#include "mode.h"

/** Holds an entry for a ban list, exemption list, or invite list.
 * This class contains a single element in a channel list, such as a banlist.
 */
class HostItem
{
 public:
	/** Time the item was added
	 */
	time_t set_time;
	/** Who added the item
	 */
	std::string set_by;
	/** The actual item data
	 */
	std::string data;

	HostItem() { /* stub */ }
	virtual ~HostItem() { /* stub */ }
};

/** A subclass of HostItem designed to hold channel bans (+b)
 */
class BanItem : public HostItem
{
};

/** Holds all relevent information for a channel.
 * This class represents a channel, and contains its name, modes, topic, topic set time,
 * etc, and an instance of the BanList type.
 */
class CoreExport Channel : public Extensible, public InviteBase
{
	/** Connect a Channel to a User
	 */
	static Channel* ForceChan(Channel* Ptr, User* user, const std::string &privs, bool bursting, bool created);

	/** Set default modes for the channel on creation
	 */
	void SetDefaultModes();

	/** Maximum number of bans (cached)
	 */
	int maxbans;

	/** Modes for the channel.
	 * This is not a null terminated string! It is a bitset where
	 * each item in it represents if a mode is set. For example
	 * for mode +A, index 0. Use modechar-65 to calculate which
	 * field to check.
	 */
	std::bitset<64> modes;

	/** Parameters for custom modes.
	 * One for each custom mode letter.
	 */
	CustomModeList custom_mode_params;

 public:
	/** Creates a channel record and initialises it with default values
	 * @throw Nothing at present.
	 */
	Channel(const std::string &name, time_t ts);

	/** The channel's name.
	 */
	std::string name;

	/** Time that the object was instantiated (used for TS calculation etc)
	*/
	time_t age;

	/** User list.
	 */
	UserMembList userlist;

	/** Channel topic.
	 * If this is an empty string, no channel topic is set.
	 */
	std::string topic;

	/** Time topic was set.
	 * If no topic was ever set, this will be equal to Channel::created
	 */
	time_t topicset;

	/** The last user to set the topic.
	 * If this member is an empty string, no topic was ever set.
	 */
	std::string setby; /* 128 */

	/** The list of all bans set on the channel.
	 */
	BanList bans;

	/** Sets or unsets a custom mode in the channels info
	 * @param mode The mode character to set or unset
	 * @param value True if you want to set the mode or false if you want to remove it
	 */
	void SetMode(ModeHandler* mode, bool value);
	void SetMode(char mode,bool mode_on);

	/** Sets or unsets a custom mode in the channels info
	 * @param mode The mode character to set or unset
	 * @param parameter The parameter string to associate with this mode character.
	 * If it is empty, the mode is unset; if it is nonempty, the mode is set.
	 */
	void SetModeParam(ModeHandler* mode, const std::string& parameter);
	void SetModeParam(char mode, const std::string& parameter);

	/** Returns true if a mode is set on a channel
	  * @param mode The mode character you wish to query
	  * @return True if the custom mode is set, false if otherwise
	  */
	inline bool IsModeSet(char mode) { return modes[mode-'A']; }
	inline bool IsModeSet(ModeHandler* mode) { return modes[mode->GetModeChar()-'A']; }


	/** Returns the parameter for a custom mode on a channel.
	  * @param mode The mode character you wish to query
	  *
	  * For example if "+L #foo" is set, and you pass this method
	  * 'L', it will return '\#foo'. If the mode is not set on the
	  * channel, or the mode has no parameters associated with it,
	  * it will return an empty string.
	  *
	  * @return The parameter for this mode is returned, or an empty string
	  */
	std::string GetModeParameter(char mode);
	std::string GetModeParameter(ModeHandler* mode);

	/** Sets the channel topic.
	 * @param u The user setting the topic
	 * @param t The topic to set it to. Non-const, as it may be modified by a hook.
	 * @param forceset If set to true then all access checks will be bypassed.
	 */
	int SetTopic(User *u, std::string &t, bool forceset = false);

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
	Membership* AddUser(User* user);

	/** Delete a user pointer to the internal reference list
	 * @param user The user to delete
	 * @return number of users left on the channel after deletion of the user
	 */
	void DelUser(User* user);

	/** Obtain the internal reference list
	 * The internal reference list contains a list of User*.
	 * These are used for rapid comparison to determine
	 * channel membership for PRIVMSG, NOTICE, QUIT, PART etc.
	 * The resulting pointer to the vector should be considered
	 * readonly and only modified via AddUser and DelUser.
	 *
	 * @return This function returns pointer to a map of User pointers (CUList*).
	 */
	const UserMembList* GetUsers();

	/** Returns true if the user given is on the given channel.
	 * @param user The user to look for
	 * @return True if the user is on this channel
	 */
	bool HasUser(User* user);

	Membership* GetUser(User* user);

	/** Make src kick user from this channel with the given reason.
	 * @param src The source of the kick
	 * @param user The user being kicked (must be on this channel)
	 * @param reason The reason for the kick
	 */
	void KickUser(User *src, User *user, const char* reason);

	/** Part a user from this channel with the given reason.
	 * If the reason field is NULL, no reason will be sent.
	 * @param user The user who is parting (must be on this channel)
	 * @param reason The part reason
	 */
	void PartUser(User *user, std::string &reason);

	/* Join a user to a channel. May be a channel that doesnt exist yet.
	 * @param user The user to join to the channel.
	 * @param cn The channel name to join to. Does not have to exist.
	 * @param key The key of the channel, if given
	 * @param override If true, override all join restrictions such as +bkil
	 * @return A pointer to the Channel the user was joined to. A new Channel may have
	 * been created if the channel did not exist before the user was joined to it.
	 * If the user could not be joined to a channel, the return value may be NULL.
	 */
	static Channel* JoinUser(User *user, const char* cn, bool override, const char* key, bool bursting, time_t TS = 0);

	/** Write to a channel, from a user, using va_args for text
	 * @param user User whos details to prefix the line with
	 * @param text A printf-style format string which builds the output line without prefix
	 * @param ... Zero or more POD types
	 */
	void WriteChannel(User* user, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Write to a channel, from a user, using std::string for text
	 * @param user User whos details to prefix the line with
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteChannel(User* user, const std::string &text);

	/** Write to a channel, from a server, using va_args for text
	 * @param ServName Server name to prefix the line with
	 * @param text A printf-style format string which builds the output line without prefix
	 * @param ... Zero or more POD type
	 */
	void WriteChannelWithServ(const std::string& ServName, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Write to a channel, from a server, using std::string for text
	 * @param ServName Server name to prefix the line with
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteChannelWithServ(const std::string& ServName, const std::string &text);

	/** Write to all users on a channel except a specific user, using va_args for text.
	 * Internally, this calls WriteAllExcept().
	 * @param user User whos details to prefix the line with, and to omit from receipt of the message
	 * @param serversource If this parameter is true, use the local server name as the source of the text, otherwise,
	 * use the nick!user\@host of the user.
	 * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
	 * @param text A printf-style format string which builds the output line without prefix
	 * @param ... Zero or more POD type
	 */
	void WriteAllExceptSender(User* user, bool serversource, char status, const char* text, ...) CUSTOM_PRINTF(5, 6);

	/** Write to all users on a channel except a list of users, using va_args for text
	 * @param user User whos details to prefix the line with, and to omit from receipt of the message
	 * @param serversource If this parameter is true, use the local server name as the source of the text, otherwise,
	 * use the nick!user\@host of the user.
	 * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
	 * @param except_list A list of users NOT to send the text to
	 * @param text A printf-style format string which builds the output line without prefix
	 * @param ... Zero or more POD type
	 */
	void WriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const char* text, ...) CUSTOM_PRINTF(6, 7);

	/** Write to all users on a channel except a specific user, using std::string for text.
	 * Internally, this calls WriteAllExcept().
	 * @param user User whos details to prefix the line with, and to omit from receipt of the message
	 * @param serversource If this parameter is true, use the local server name as the source of the text, otherwise,
	 * use the nick!user\@host of the user.
	 * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteAllExceptSender(User* user, bool serversource, char status, const std::string& text);

	/** Write to all users on a channel except a list of users, using std::string for text
	 * @param user User whos details to prefix the line with, and to omit from receipt of the message
	 * @param serversource If this parameter is true, use the local server name as the source of the text, otherwise,
	 * use the nick!user\@host of the user.
	 * @param status The status of the users to write to, e.g. '@' or '%'. Use a value of 0 to write to everyone
	 * @param except_list A list of users NOT to send the text to
	 * @param text A std::string containing the output line without prefix
	 */
	void WriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string& text);
	/** Write a line of text that already includes the source */
	void RawWriteAllExcept(User* user, bool serversource, char status, CUList &except_list, const std::string& text);

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
	 * @param user The user to spool the NAMES list to
	 */
	void UserList(User *user);

	/** Get the number of invisible users on this channel
	 * @return Number of invisible users
	 */
	int CountInvisible();

	/** Get a users prefix on this channel in a string.
	 * @param user The user to look up
	 * @return A character array containing the prefix string.
	 * Unlike GetStatus and GetStatusFlags which will only return the
	 * core specified modes @, % and + (op, halfop and voice), GetPrefixChar
	 * will also return module-defined prefixes. If the user has to prefix,
	 * an empty but non-null string is returned. If the user has multiple
	 * prefixes, the highest is returned. If you do not recognise the prefix
	 * character you can get, you can deal with it in a 'proprtional' manner
	 * compared to known prefixes, using GetPrefixValue().
	 */
	const char* GetPrefixChar(User *user);

	/** Return all of a users mode prefixes into a char* string.
	 * @param user The user to look up
	 * @return A list of all prefix characters. The prefixes will always
	 * be in rank order, greatest first, as certain IRC clients require
	 * this when multiple prefixes are used names lists.
	 */
	const char* GetAllPrefixChars(User* user);

	/** Get the value of a users prefix on this channel.
	 * @param user The user to look up
	 * @return The module or core-defined value of the users prefix.
	 * The values for op, halfop and voice status are constants in
	 * mode.h, and are OP_VALUE, HALFOP_VALUE, and VOICE_VALUE respectively.
	 * If the value you are given does not match one of these three, it is
	 * a module-defined mode, and it should be compared in proportion to
	 * these three constants. For example a value greater than OP_VALUE
	 * is a prefix of greater 'worth' than ops, and a value less than
	 * VOICE_VALUE is of lesser 'worth' than a voice.
	 */
	unsigned int GetPrefixValue(User* user);

	/** This method removes all prefix characters from a user.
	 * It will not inform the user or the channel of the removal of prefixes,
	 * and should be used when the user parts or quits.
	 * @param user The user to remove all prefixes from
	 */
	void RemoveAllPrefixes(User* user);

	/** Add a prefix character to a user.
	 * Only the core should call this method, usually  from
	 * within the mode parser or when the first user joins
	 * the channel (to grant ops to them)
	 * @param user The user to associate the privilage with
	 * @param prefix The prefix character to associate
	 * @param adding True if adding the prefix, false when removing
	 * @return True if a change was made
	 */
	bool SetPrefix(User* user, char prefix, bool adding);

	/** Check if a user is banned on this channel
	 * @param user A user to check against the banlist
	 * @returns True if the user given is banned
	 */
	bool IsBanned(User* user);

	/** Check a single ban for match
	 */
	bool CheckBan(User* user, const std::string& banmask);

	/** Get the status of an "action" type extban
	 */
	ModResult GetExtBanStatus(User *u, char type);

	/** Clears the cached max bans value
	 */
	void ResetMaxBans();
};

#endif
