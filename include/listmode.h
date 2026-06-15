/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2019, 2021-2023 Sadie Powell <sadie@sadiepowell.dev>
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

#include "extension.h"

/** The base class for list modes, should be inherited.
 */
class CoreExport ListModeBase
	: public ModeHandler
{
public:
	/** An item in a listmode's list
	 */
	struct ListItem final
	{
		std::string setter;
		std::string mask;
		time_t time;
		ListItem(const std::string& Mask, const std::string& Setter, time_t Time)
			: setter(Setter)
			, mask(Mask)
			, time(Time)
		{
		}
	};

	/** Items stored in the channel's list
	 */
	using ModeList = std::vector<ListItem>;

	/** The default maximum list size. */
	static constexpr size_t DEFAULT_LIST_SIZE = 100;

private:
	class ChanData final
	{
	public:
		/* The entries on the list mode list. */
		ModeList list;

		/** The time at which maxitems was last checked. */
		time_t maxchecked = 0;

		/** The maximum number of entries on the list mode list. */
		size_t maxentries = 0;
	};

	/** Finds the maximum number of this list mode that can be placed on the
	 * given channel name according to the \<maxlimit> config.
	 * @param channame The channel name to find the limit for.
	 * @return The maximum number of modes of this type that we allow to be se
	 *         on the given channel name
	 */
	size_t FindLimit(const Channel* chan) const;

protected:
	/** Storage key
	 */
	SimpleExtItem<ChanData> extItem;

public:
	/** Whether the list mode accepts a user mask. */
	const bool accepts_mask;

	/** Numeric to use when outputting the list. */
	const unsigned int listnumeric;

	/** Numeric to indicate end of list. */
	const unsigned int endoflistnumeric;


	/** Constructor.
	 * @param Creator The creator of this class
	 * @param Name Mode name
	 * @param modechar Mode character
	 * @param lnum List numeric
	 * @param eolnum End of list numeric
	 * @param am Whether the list mode accepts a user mask.
	 */
	ListModeBase(const WeakModulePtr& Creator, const std::string& Name, char modechar, unsigned int lnum, unsigned int eolnum, bool am = false);

	/** Compares an entry from this list with the specified value.
	 * @param entry The list entry to compare against.
	 * @param value The value to compare to.
	 * @return If the entries are equivalent then true; otherwise, false.
	 */
	virtual bool CompareEntry(const std::string& entry, const std::string& value) const { return entry == value; }

	/** Get limit of this mode on a channel
	 * @param channel The channel to inspect
	 * @return Maximum number of modes of this type that can be placed on the given channel
	 */
	size_t GetLimit(Channel* chan, ChanData *data = nullptr);

	/** Retrieves the list of all modes set on the given channel
	 * @param channel Channel to get the list from
	 * @return A list with all modes of this type set on the given channel, can be NULL
	 */
	ModeList* GetList(Channel* channel);

	/** @copydoc ModeHandler::DisplayList */
	void DisplayList(User* user, Channel* channel) override;

	/** @copydoc ModeHandler::DisplayEmptyList */
	void DisplayEmptyList(User* user, Channel* channel) override;

	/** @copydoc ModeHandler::RemoveMode(Channel*,Modes::ChangeList&) */
	void RemoveMode(Channel* channel, Modes::ChangeList& changelist) override;

	/** @copydoc ModeHandler::OnModeChange */
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;

	/** Validate a potential entry for this list mode. This is only for local validation, you
	 * should be prepared to handle malformed entries from remote servers as there is no good
	 * way to prevent them from being set without causing a desync.
	 * @param user The local user which sent the list mode change.
	 * @param channel The channel the mode is being changed on.
	 * @param parameter The parameter that the user specified.
	 * @return True if the parameter is valid; otherwise, false.
	 */
	virtual bool ValidateParam(LocalUser* user, Channel* channel, std::string& parameter);

	/** @copydoc ModeHandler::OnParameterMissing */
	void OnParameterMissing(User* user, User* dest, Channel* channel) override;

	/** Tell the user the list is too long.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 * @param limit The list limit for the channel.
	 */
	virtual void TellListTooLong(LocalUser* source, Channel* channel, const std::string& parameter, size_t limit);

	/** Tell the user an item is already on the list.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 */
	virtual void TellAlreadyOnList(LocalUser* source, Channel* channel, const std::string& parameter);

	/** Tell the user that the parameter is not in the list.
	 * Overridden by implementing module.
	 * @param source Source user removing the parameter
	 * @param channel Channel the parameter is being removed from
	 * @param parameter The actual parameter being removed
	 */
	virtual void TellNotSet(LocalUser* source, Channel* channel, const std::string& parameter);
};

inline ListModeBase::ModeList* ListModeBase::GetList(Channel* channel)
{
	ChanData* cd = extItem.Get(channel);
	if (!cd)
		return nullptr;

	return &cd->list;
}
