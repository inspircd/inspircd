/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2019 Sadie Powell <sadie@witchery.services>
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
class CoreExport ListModeBase : public ModeHandler {
  public:
    /** An item in a listmode's list
     */
    struct ListItem {
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
    class ChanData {
      public:
        ModeList list;
        int maxitems;

        ChanData() : maxitems(-1) { }
    };

    /** The number of items a listmode's list may contain
     */
    struct ListLimit {
        std::string mask;
        unsigned int limit;
        ListLimit(const std::string& Mask, unsigned int Limit) : mask(Mask),
            limit(Limit) { }
        bool operator==(const ListLimit& other) const {
            return (this->mask == other.mask && this->limit == other.limit);
        }
    };

    /** Max items per channel by name
     */
    typedef std::vector<ListLimit> limitlist;

    /** The default maximum list size. */
    static const unsigned int DEFAULT_LIST_SIZE = 100;

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

    /** Limits on a per-channel basis read from the \<listmode>
     * config tag.
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
     */
    ListModeBase(Module* Creator, const std::string& Name, char modechar,
                 const std::string& eolstr, unsigned int lnum, unsigned int eolnum,
                 bool autotidy);

    /** Determines whether some channels have longer lists than others. */
    bool HasVariableLength() const {
        return chanlimits.size() > 1;
    }

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
    void DisplayList(User* user, Channel* channel) CXX11_OVERRIDE;

    /** Tell a user that a list contains no elements.
     * Sends 'eolnum' numeric with text 'eolstr', unless overridden (see constructor)
     * @param user The user issuing the command
     * @param channel The channel that has the empty list
     * See mode.h
     */
    void DisplayEmptyList(User* user, Channel* channel) CXX11_OVERRIDE;

    /** Remove all instances of the mode from a channel.
     * Populates the given modestack with modes that remove every instance of
     * this mode from the channel.
     * See mode.h for more details.
     * @param channel The channel to remove all instances of the mode from
     * @param changelist Mode change list to populate with the removal of this mode
     */
    void RemoveMode(Channel* channel, Modes::ChangeList& changelist) CXX11_OVERRIDE;

    /** Perform a rehash of this mode's configuration data
     */
    void DoRehash();

    /** Handle the list mode.
     * See mode.h
     */
    ModeAction OnModeChange(User* source, User*, Channel* channel,
                            std::string &parameter, bool adding) CXX11_OVERRIDE;

    /** Validate parameters.
     * Overridden by implementing module.
     * @param user Source user adding the parameter
     * @param channel Channel the parameter is being added to
     * @param parameter The actual parameter being added
     * @return true if the parameter is valid
     */
    virtual bool ValidateParam(User* user, Channel* channel,
                               std::string& parameter);

    /** In the event that the mode should be given a parameter, and no parameter was provided, this method is called.
     * This allows you to give special information to the user, or handle this any way you like.
     * @param user The user issuing the mode change
     * @param dest For user mode changes, the target of the mode. For channel mode changes, NULL.
     * @param channel For channel mode changes, the target of the mode. For user mode changes, NULL.
     * See mode.h
     */
    virtual void OnParameterMissing(User* user, User* dest,
                                    Channel* channel) CXX11_OVERRIDE;

    /** Tell the user the list is too long.
     * Overridden by implementing module.
     * @param source Source user adding the parameter
     * @param channel Channel the parameter is being added to
     * @param parameter The actual parameter being added
     */
    virtual void TellListTooLong(User* source, Channel* channel,
                                 std::string& parameter);

    /** Tell the user an item is already on the list.
     * Overridden by implementing module.
     * @param source Source user adding the parameter
     * @param channel Channel the parameter is being added to
     * @param parameter The actual parameter being added
     */
    virtual void TellAlreadyOnList(User* source, Channel* channel,
                                   std::string& parameter);

    /** Tell the user that the parameter is not in the list.
     * Overridden by implementing module.
     * @param source Source user removing the parameter
     * @param channel Channel the parameter is being removed from
     * @param parameter The actual parameter being removed
     */
    virtual void TellNotSet(User* source, Channel* channel, std::string& parameter);
};

inline ListModeBase::ModeList* ListModeBase::GetList(Channel* channel) {
    ChanData* cd = extItem.get(channel);
    if (!cd) {
        return NULL;
    }

    return &cd->list;
}
