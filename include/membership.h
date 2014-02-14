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

class CoreExport Membership : public Extensible, public intrusive_list_node<Membership>
{
 public:
	User* const user;
	Channel* const chan;
	// mode list, sorted by prefix rank, higest first
	std::string modes;
	Membership(User* u, Channel* c) : user(u), chan(c) {}
	inline bool hasMode(char m) const
	{
		return modes.find(m) != std::string::npos;
	}
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
	intrusive_list<Invitation, T> invites;

 public:
	void ClearInvites();

	friend class Invitation;
};

class CoreExport Invitation : public intrusive_list_node<Invitation, Channel>, public intrusive_list_node<Invitation, LocalUser>
{
	Invitation(Channel* c, LocalUser* u, time_t timeout) : user(u), chan(c), expiry(timeout) {}

 public:
	LocalUser* const user;
	Channel* const chan;
	time_t expiry;

	~Invitation();
	static void Create(Channel* c, LocalUser* u, time_t timeout);
	static Invitation* Find(Channel* c, LocalUser* u, bool check_expired = true);
};

typedef intrusive_list<Invitation, LocalUser> InviteList;

template<typename T>
inline void InviteBase<T>::ClearInvites()
{
	for (typename intrusive_list<Invitation, T>::iterator i = invites.begin(); i != invites.end(); )
	{
		Invitation* inv = *i;
		++i;
		delete inv;
	}
}
