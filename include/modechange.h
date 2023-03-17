/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2018 Attila Molnar <attilamolnar@hush.com>
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

namespace Modes {
struct Change;
class ChangeList;
}

/** A single mode to be changed
 */
struct Modes::Change {
    bool adding;
    ModeHandler* mh;
    std::string param;

    /**
     * @param handler Mode handler
     * @param add True if this mode is being set, false if removed
     * @param parameter Mode parameter
     */
    Change(ModeHandler* handler, bool add, const std::string& parameter)
        : adding(add)
        , mh(handler)
        , param(parameter) {
    }
};

/** A list of mode changes that can be applied on a Channel or User
 */
class Modes::ChangeList {
  public:
    typedef std::vector<Change> List;

    /** Add a new mode to be changed to this ChangeList
     * @param change Mode change to add
     */
    void push(const Modes::Change& change) {
        items.push_back(change);
    }

    /** Insert multiple mode changes to the ChangeList
     * @param first Iterator to the first change to insert
     * @param last Iterator to the first change to not insert
     */
    void push(List::const_iterator first, List::const_iterator last) {
        items.insert(items.end(), first, last);
    }

    /** Add a new mode to be changed to this ChangeList
     * @param mh Mode handler
     * @param adding True if this mode is being set, false if removed
     * @param param Mode parameter
     */
    void push(ModeHandler* mh, bool adding,
              const std::string& param = std::string()) {
        items.push_back(Change(mh, adding, param));
    }

    /** Add a new mode to this ChangeList which will be set on the target
     * @param mh Mode handler
     * @param param Mode parameter
     */
    void push_add(ModeHandler* mh, const std::string& param = std::string()) {
        push(mh, true, param);
    }

    /** Add a new mode to this ChangeList which will be unset from the target
     * @param mh Mode handler
     * @param param Mode parameter
     */
    void push_remove(ModeHandler* mh, const std::string& param = std::string()) {
        push(mh, false, param);
    }

    /** Remove all mode changes from this stack
     */
    void clear() {
        items.clear();
    }

    /** Checks whether the ChangeList is empty, equivalent to (size() != 0).
     * @return True if the ChangeList is empty, false otherwise.
     */
    bool empty() const {
        return items.empty();
    }

    /** Get number of mode changes in this ChangeList
     * @return Number of mode changes in this ChangeList
     */
    List::size_type size() const {
        return items.size();
    }

    /** Get the list of mode changes in this ChangeList
     * @return List of modes added to this ChangeList
     */
    const List& getlist() const {
        return items;
    }

    /** Get the list of mode changes in this ChangeList
     * @return List of modes added to this ChangeList
     */
    List& getlist() {
        return items;
    }

  private:
    List items;
};
