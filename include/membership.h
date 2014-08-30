/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

uint64_t ConvToUInt64(const std::string& in);

/**
 * Represents a member of a channel.
 * A Membership object is created when a user joins a channel, and destroyed when a user leaves
 * (via kick, part or quit) a channel.
 * All prefix modes a member has is tracked by this object. Moreover, Memberships are Extensibles
 * meaning modules can add arbitrary data to them using extensions (see m_delaymsg for an example).
 */
class CoreExport Membership : public Extensible, public insp::intrusive_list_node<Membership>
{
 public:
	/** Type of the Membership id
	 */
	typedef uint64_t Id;

	/** User on the channel
	 */
	User* const user;

	/** Channel the user is on
	 */
	Channel* const chan;

	/** List of prefix mode letters this member has,
	 * sorted by prefix rank, highest first
	 */
	std::string modes;

	/** Id of this Membership, set by the protocol module, other components should never read or
	 * write this field.
	 */
	Id id;

	/** Converts a string to a Membership::Id
	 * @param str The string to convert
	 * @return Raw value of type Membership::Id
	 */
	static Id IdFromString(const std::string& str)
	{
		return ConvToUInt64(str);
	}

	/** Constructor, sets the user and chan fields to the parameters, does NOT update any bookkeeping
	 * information in the User or the Channel.
	 * Call Channel::JoinUser() or ForceJoin() to make a user join a channel instead of constructing
	 * Membership objects directly.
	 */
	Membership(User* u, Channel* c) : user(u), chan(c) {}

	/** Returns true if this member has a given prefix mode set
	 * @param m The prefix mode letter to check
	 * @return True if the member has the prefix mode set, false otherwise
	 */
	inline bool hasMode(char m) const
	{
		return modes.find(m) != std::string::npos;
	}

	/** Returns the rank of this member.
	 * The rank of a member is defined as the rank given by the 'strongest' prefix mode a
	 * member has. See the PrefixMode class description for more info.
	 * @return The rank of the member
	 */
	unsigned int getRank();

	/** Add a prefix character to a user.
	 * Only the core should call this method, usually from
	 * within the mode parser or when the first user joins
	 * the channel (to grant the default privs to them)
	 * @param mh The mode handler of the prefix mode to associate
	 * @param adding True if adding the prefix, false when removing
	 * @return True if a change was made
	 */
	bool SetPrefix(PrefixMode* mh, bool adding);

	/** Get the highest prefix this user has on the channel
	 * @return A character containing the highest prefix.
	 * If the user has no prefix, 0 is returned. If the user has multiple prefixes,
	 * the highest is returned. If you do not recognise the prefix character you
	 * can get, you can deal with it in a 'proportional' manner compared to known
	 * prefixes, using GetPrefixValue().
	 */
	char GetPrefixChar() const;

	/** Return all prefix chars this member has.
	 * @return A list of all prefix characters. The prefixes will always
	 * be in rank order, greatest first, as certain IRC clients require
	 * this when multiple prefixes are used names lists.
	 */
	const char* GetAllPrefixChars() const;
};

template <typename T>
class InviteBase
{
 protected:
	/** List of pending Invitations
	 */
	insp::intrusive_list<Invitation, T> invites;

 public:
	/** Remove and destruct all pending invitations this user or channel has.
	 * Must be called before the object is destroyed, also called when the TS of the channel is lowered.
	 */
	void ClearInvites();

	friend class Invitation;
};

/**
 * The Invitation class contains all data about a pending invitation.
 * Invitation objects are referenced from the user and the channel they belong to.
 */
class CoreExport Invitation : public insp::intrusive_list_node<Invitation, Channel>, public insp::intrusive_list_node<Invitation, LocalUser>
{
	/** Constructs an Invitation, only called by Create()
	 * @param c Channel the user is invited to
	 * @param u User being invited
	 * @param timeout Expiration time for this Invitation
	 */
	Invitation(Channel* c, LocalUser* u, time_t timeout) : user(u), chan(c), expiry(timeout) {}

 public:
	/** User the invitation is for
	 */
	LocalUser* const user;

	/** Channel where the user is invited to
	 */
	Channel* const chan;

	/** Timestamp when this Invitation expires or 0 if it doesn't expire.
	 * Invitation::Create() can update this field; see that for more info.
	 */
	time_t expiry;

	/** Destructor
	 * Removes references to this Invitation from the associated user and channel.
	 */
	~Invitation();

	/** Create or extend an Invitation.
	 * When a user is invited to join a channel either a new Invitation object is created or
	 * or the expiration timestamp is updated if there is already a pending Invitation for
	 * the given (user, channel) pair and the new expiration time is further than the current.
	 * @param c Target channel
	 * @param u Target user
	 * @param timeout Timestamp when the invite should expire, 0 for no expiration
	 */
	static void Create(Channel* c, LocalUser* u, time_t timeout);

	/** Finds the Invitation object for the given channel/user pair.
	 * @param c Target channel, can be NULL to remove expired entries
	 * @param u Target user, cannot be NULL
	 * @param check_expired Pass true to remove all expired invites found while searching, false
	 * to return with an Invitation even if it's expired
	 * @return Invitation object for the given (channel, user) pair if it exists, NULL otherwise
	 */
	static Invitation* Find(Channel* c, LocalUser* u, bool check_expired = true);
};

typedef insp::intrusive_list<Invitation, LocalUser> InviteList;

template<typename T>
inline void InviteBase<T>::ClearInvites()
{
	for (typename insp::intrusive_list<Invitation, T>::iterator i = invites.begin(); i != invites.end(); )
	{
		Invitation* inv = *i;
		// Destructing the Invitation invalidates the iterator, so move it now
		++i;
		delete inv;
	}
}
