/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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

namespace Invite
{
	class APIBase;
	class API;
	class Invite;


	/** Used to indicate who we announce invites to on a channel. */
	enum AnnounceState
	{
		/** Don't send invite announcements. */
		ANNOUNCE_NONE,

		/** Send invite announcements to all users. */
		ANNOUNCE_ALL,

		/** Send invite announcements to channel operators and higher. */
		ANNOUNCE_OPS,

		/** Send invite announcements to channel half-operators (if available) and higher. */
		ANNOUNCE_DYNAMIC
	};

	typedef insp::intrusive_list<Invite, LocalUser> List;
}

class Invite::APIBase
	: public DataProvider
{
public:
	APIBase(Module* parent);

	/** Create or extend an Invite.
	 * When a user is invited to join a channel either a new Invite object is created or
	 * or the expiration timestamp is updated if there is already a pending Invite for
	 * the given (user, channel) pair and the new expiration time is further than the current.
	 * @param user Target user
	 * @param chan Target channel
	 * @param timeout Timestamp when the invite should expire, 0 for no expiration
	 */
	virtual void Create(LocalUser* user, Channel* chan, time_t timeout) = 0;

	/** Retrieves the Invite object for the given (user, channel) pair
	 * @param user Target user
	 * @param chan Target channel
	 * @return Invite object for the given (channel, user) pair if it exists, NULL otherwise
	 */
	virtual Invite* Find(LocalUser* user, Channel* chan) = 0;

	/** Returns the announcement behaviour for invites from <security:announceinvites>. */
	virtual AnnounceState GetAnnounceState() const = 0;

	/** Returns the list of channels a user has been invited to but has not yet joined.
	 * @param user User whose invite list to retrieve
	 * @return List of channels the user is invited to or NULL if the list is empty
	 */
	virtual const List* GetList(LocalUser* user) = 0;

	/** Check if a user is invited to a channel
	 * @param user User to check
	 * @param chan Channel to check
	 * @return True if the user is invited to the channel, false otherwise
	 */
	bool IsInvited(LocalUser* user, Channel* chan) { return (Find(user, chan) != nullptr); }

	/** Removes an Invite if it exists
	 * @param user User whose invite to remove
	 * @param chan Channel to remove the invite to
	 * @return True if the user was invited to the channel and the invite was removed, false if the user wasn't invited
	 */
	virtual bool Remove(LocalUser* user, Channel* chan) = 0;
};

class Invite::API final
	: public dynamic_reference<APIBase>
{
public:
	API(Module* parent)
		: dynamic_reference<APIBase>(parent, "core_channel_invite")
	{
	}
};

/**
 * The Invite class contains all data about a pending invite.
 * Invite objects are referenced from the user and the channel they belong to.
 */
class Invite::Invite final
	: public insp::intrusive_list_node<Invite, LocalUser>, public insp::intrusive_list_node<Invite, Channel>
{
public:
	/** User the invite is for
	 */
	LocalUser* const user;

	/** Channel where the user is invited to
	 */
	Channel* const chan;

	/** Check whether the invite will expire or not
	 * @return True if the invite is timed, false if it doesn't expire
	 */
	bool IsTimed() const { return (expiretimer != nullptr); }

	/** Serialize this object
	 * @param human Whether to serialize for human consumption or not.
	 * @param show_chans True to include channel in the output, false to include the nick/uuid
	 * @param out Output will be appended to this string
	 */
	void Serialize(bool human, bool show_chans, std::string& out);

	friend class APIImpl;

private:
	/** Timer handling expiration. If NULL this invite doesn't expire.
	 */
	Timer* expiretimer = nullptr;

	/** Constructor, only available to the module providing the invite API (core_channel).
	 * To create Invites use InviteAPI::Create().
	 * @param user User being invited
	 * @param chan Channel the user is invited to
	 */
	Invite(LocalUser* user, Channel* chan);

	/** Destructor, only available to the module providing the invite API (core_channel).
	 * To remove Invites use InviteAPI::Remove().
	 */
	~Invite();
};
