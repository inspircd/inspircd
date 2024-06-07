/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018, 2021 Sadie Powell <sadie@witchery.services>
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

namespace Modes
{
	struct Change;
	class ChangeList;
}

/** A single mode to be changed
 */
struct Modes::Change final
{
	bool adding;
	ModeHandler* mh;
	std::string param;
	std::optional<std::string> set_by;
	std::optional<time_t> set_at;

	/**
	 * @param handler Mode handler
	 * @param add True if this mode is being set, false if removed
	 * @param parameter Mode parameter
	 */
	Change(ModeHandler* handler, bool add, const std::string& parameter = "")
		: adding(add)
		, mh(handler)
		, param(parameter)
	{
	}

	/**
	 * @param handler Mode handler
	 * @param add True if this mode is being set, false if removed
	 * @param parameter Mode parameter
	 * @param setby Who the mode change was originally performed by.
	 * @param setat When the mode change was originally made.
	 */
	Change(ModeHandler* handler, bool add, const std::string& parameter, const std::string& setby, time_t setat)
		: adding(add)
		, mh(handler)
		, param(parameter)
		, set_by(setby)
		, set_at(setat)
	{
	}
};

/** A list of mode changes that can be applied on a Channel or User
 */
class Modes::ChangeList final
{
public:
	typedef std::vector<Change> List;

	/** Add a new mode to be changed to this ChangeList
	 * @param change Mode change to add
	 */
	void push(const Modes::Change& change)
	{
		items.push_back(change);
	}

	/** Insert multiple mode changes to the ChangeList
	 * @param first Iterator to the first change to insert
	 * @param last Iterator to the first change to not insert
	 */
	void push(List::const_iterator first, List::const_iterator last)
	{
		items.insert(items.end(), first, last);
	}

	/** Add a new mode to be changed to this ChangeList. Parameters are forwarded to the Modes::Change constructor. */
	template <typename... Args>
	void push(Args&&... args)
	{
		items.emplace_back(std::forward<Args>(args)...);
	}

	/** Add a new mode to this ChangeList which will be set on the target
	 * @param mh Mode handler
	 * @param param Mode parameter
	 */
	void push_add(ModeHandler* mh, const std::string& param = std::string())
	{
		push(mh, true, param);
	}

	/** Add a new mode to this ChangeList which will be unset from the target
	 * @param mh Mode handler
	 * @param param Mode parameter
	 */
	void push_remove(ModeHandler* mh, const std::string& param = std::string())
	{
		push(mh, false, param);
	}

	/** Remove all mode changes from this stack
	 */
	void clear() { items.clear(); }

	/** Checks whether the ChangeList is empty, equivalent to (size() != 0).
	 * @return True if the ChangeList is empty, false otherwise.
	 */
	bool empty() const { return items.empty(); }

	/** Get number of mode changes in this ChangeList
	 * @return Number of mode changes in this ChangeList
	 */
	List::size_type size() const { return items.size(); }

	/** Get the list of mode changes in this ChangeList
	 * @return List of modes added to this ChangeList
	 */
	const List& getlist() const { return items; }

	/** Get the list of mode changes in this ChangeList
	 * @return List of modes added to this ChangeList
	 */
	List& getlist() { return items; }

private:
	List items;
};
