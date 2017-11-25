/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/**
 * The CullList class is used to delete objects at the end of the main loop to
 * avoid problems with references to deleted pointers if an object were deleted
 * during execution.
 */
class CoreExport CullList
{
	std::vector<classbase*> list;
	std::vector<LocalUser*> SQlist;

 public:
	/** Adds an item to the cull list
	 */
	void AddItem(classbase* item) { list.push_back(item); }
	void AddSQItem(LocalUser* item) { SQlist.push_back(item); }

	/** Applies the cull list (deletes the contents)
	 */
	void Apply();
};

/** Represents an action which is executable by an action list */
class CoreExport ActionBase : public classbase
{
 public:
	 /** Executes this action. */
	virtual void Call() = 0;
};

class CoreExport ActionList
{
	std::vector<ActionBase*> list;

 public:
	/** Adds an item to the list
	 */
	void AddAction(ActionBase* item) { list.push_back(item); }

	/** Runs the items
	 */
	void Run();

};
