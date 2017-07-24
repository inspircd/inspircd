/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

/** The base class for list modes, should be inherited.
 */
class CoreExport ListModeBase : public ModeHandler
{
 public:
	/** An item in a listmode's list
	 */
	struct ListItem
	{
		std::string setter;
		std::string mask;
		time_t time;
		ListItem(const std::string& Mask, const std::string& Setter, time_t Time)
			: setter(Setter), mask(Mask), time(Time) { }
	};

	/** Items stored in the channel's list
	 */
	typedef std::vector<ListItem> ModeList;

 private:
	class ChanData
	{
	public:
		ModeList list;
		int maxitems;

		ChanData() : maxitems(-1) { }
	};

	/** The number of items a listmode's list may contain
	 */
	struct ListLimit
	{
		std::string mask;
		unsigned int limit;
		ListLimit(const std::string& Mask, unsigned int Limit) : mask(Mask), limit(Limit) { }
		bool operator==(const ListLimit& other) const { return (this->mask == other.mask && this->limit == other.limit); }
	};

	/** Max items per channel by name
	 */
	typedef std::vector<ListLimit> limitlist;

	/** The default maximum list size. */
	static const unsigned int DEFAULT_LIST_SIZE = 64;

	/** Finds the limit of modes that can be placed on the given channel name according to the config
	 * @param channame The channel name to find the limit for
	 * @return The maximum number of modes of this type that we allow to be set on the given channel name
	 */
	unsigned int FindLimit(const std::string& channame);

	/** Returns the limit on the given channel for this mode.
	 * If the limit is cached then the cached value is returned,
	 * otherwise the limit is determined using FindLimit() and cached
	 * for later queries before it is returned
	 * @param channame The channel name to find the limit for
	 * @param cd The ChanData associated with channel channame
	 * @return The maximum number of modes of this type that we allow to be set on the given channel
	 */
	unsigned int GetLimitInternal(const std::string& channame, ChanData* cd);

 protected:
	/** Numeric to use when outputting the list
	 */
	unsigned int listnumeric;
	/** Numeric to indicate end of list
	 */
	unsigned int endoflistnumeric;
	/** String to send for end of list
	 */
	std::string endofliststring;
	/** Automatically tidy up entries
	 */
	bool tidy;
	/** Config tag to check for max items per channel
	 */
	std::string configtag;
	/** Limits on a per-channel basis read from the tag
	 * specified in ListModeBase::configtag
	 */
	limitlist chanlimits;

	/** Storage key
	 */
	SimpleExtItem<ChanData> extItem;

 public:
	/** Constructor.
	 * @param Creator The creator of this class
	 * @param Name Mode name
	 * @param modechar Mode character
	 * @param eolstr End of list string
	 * @param lnum List numeric
	 * @param eolnum End of list numeric
	 * @param autotidy Automatically tidy list entries on add
	 * @param ctag Configuration tag to get limits from
	 */
	ListModeBase(Module* Creator, const std::string& Name, char modechar, const std::string &eolstr, unsigned int lnum, unsigned int eolnum, bool autotidy, const std::string &ctag = "banlist");

	/** Get limit of this mode on a channel
	 * @param channel The channel to inspect
	 * @return Maximum number of modes of this type that can be placed on the given channel
	 */
	unsigned int GetLimit(Channel* channel);

	/** Gets the lower list limit for this listmode.
	 */
	unsigned int GetLowerLimit();

	/** Retrieves the list of all modes set on the given channel
	 * @param channel Channel to get the list from
	 * @return A list with all modes of this type set on the given channel, can be NULL
	 */
	ModeList* GetList(Channel* channel);

	/** Display the list for this mode
	 * See mode.h
	 * @param user The user to send the list to
	 * @param channel The channel the user is requesting the list for
	 */
	virtual void DisplayList(User* user, Channel* channel);

	/** Tell a user that a list contains no elements.
	 * Sends 'eolnum' numeric with text 'eolstr', unless overridden (see constructor)
	 * @param user The user issuing the command
	 * @param channel The channel that has the empty list
	 * See mode.h
	 */
	virtual void DisplayEmptyList(User* user, Channel* channel);

	/** Remove all instances of the mode from a channel.
	 * Populates the given modestack with modes that remove every instance of
	 * this mode from the channel.
	 * See mode.h for more details.
	 * @param channel The channel to remove all instances of the mode from
	 * @param changelist Mode change list to populate with the removal of this mode
	 */
	virtual void RemoveMode(Channel* channel, Modes::ChangeList& changelist);

	/** Perform a rehash of this mode's configuration data
	 */
	void DoRehash();

	/** Handle the list mode.
	 * See mode.h
	 */
	virtual ModeAction OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding);

	/** Validate parameters.
	 * Overridden by implementing module.
	 * @param user Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 * @return true if the parameter is valid
	 */
	virtual bool ValidateParam(User* user, Channel* channel, std::string& parameter);

	/** Tell the user the list is too long.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 */
	virtual void TellListTooLong(User* source, Channel* channel, std::string& parameter);

	/** Tell the user an item is already on the list.
	 * Overridden by implementing module.
	 * @param source Source user adding the parameter
	 * @param channel Channel the parameter is being added to
	 * @param parameter The actual parameter being added
	 */
	virtual void TellAlreadyOnList(User* source, Channel* channel, std::string& parameter);

	/** Tell the user that the parameter is not in the list.
	 * Overridden by implementing module.
	 * @param source Source user removing the parameter
	 * @param channel Channel the parameter is being removed from
	 * @param parameter The actual parameter being removed
	 */
	virtual void TellNotSet(User* source, Channel* channel, std::string& parameter);
};

inline ListModeBase::ModeList* ListModeBase::GetList(Channel* channel)
{
	ChanData* cd = extItem.get(channel);
	if (!cd)
		return NULL;

	return &cd->list;
}
