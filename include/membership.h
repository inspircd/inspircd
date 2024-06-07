/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2018, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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

#include "convto.h"

/**
 * Represents a member of a channel.
 * A Membership object is created when a user joins a channel, and destroyed when a user leaves
 * (via kick, part or quit) a channel.
 * All prefix modes a member has is tracked by this object. Moreover, Memberships are Extensibles
 * meaning modules can add arbitrary data to them using extensions (see m_delaymsg for an example).
 */
class CoreExport Membership final
	: public Extensible
	, public insp::intrusive_list_node<Membership>
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
	std::set<PrefixMode*, PrefixMode::Sorter> modes;

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
		return ConvToNum<Id>(str);
	}

	/** Constructor, sets the user and chan fields to the parameters, does NOT update any bookkeeping
	 * information in the User or the Channel.
	 * Call Channel::JoinUser() or ForceJoin() to make a user join a channel instead of constructing
	 * Membership objects directly.
	 */
	Membership(User* u, Channel* c)
		: Extensible(ExtensionType::MEMBERSHIP)
		, user(u)
		, chan(c)
	{
	}

	/** Check if this member has a given prefix mode set
	 * @param pm Prefix mode to check
	 * @return True if the member has the prefix mode set, false otherwise
	 */
	bool HasMode(const PrefixMode* pm) const
	{
		return std::find(modes.begin(), modes.end(), pm) != modes.end();
	}

	/** Returns the highest prefix mode for this membership or nullptr if no prefix mode is set. */
	PrefixMode* GetMode() const { return modes.empty() ? nullptr : *modes.begin(); }

	/** Returns the rank of this member.
	 * The rank of a member is defined as the rank given by the 'strongest' prefix mode a
	 * member has. See the PrefixMode class description for more info.
	 * @return The rank of the member
	 */
	ModeHandler::Rank GetRank() const { return modes.empty() ? 0 : (*modes.begin())->GetPrefixRank(); }

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

	/** Get the mode character of the highest prefix mode this user has on the channel or 0 if no prefix modes are set. */
	char GetModeChar() const { return modes.empty() ? 0 : (*modes.begin())->GetModeChar(); }

	/** Return all prefix chars this member has.
	 * @return A list of all prefix characters. The prefixes will always
	 * be in rank order, greatest first, as certain IRC clients require
	 * this when multiple prefixes are used names lists.
	 */
	std::string GetAllPrefixChars() const;

	/** Returns all prefix modes this member has ordered descending by rank. */
	std::string GetAllPrefixModes() const;

	/** Sends a server notice to this user in the context of this channel.
	 * @param text The contents of the message to send.
	 */
	void WriteNotice(const std::string& text) const;
};
