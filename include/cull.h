/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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

/* Allows deleting instances of an inheriting type at the end of the current main loop iteration. */
class CoreExport Cullable
	: private insp::uncopiable
{
protected:
	/** Default constructor for the Cullable class. */
	Cullable();

public:
	/** Deleter that queues an object for deletion at the end of the current main loop iteration. */
	struct Deleter final
	{
		void operator()(Cullable* item) const;
	};

	/** Dummy class to help ensure all superclasses get culled. */
	class Result final
	{
	public:
		/** Default constructor for the Cullable::Result class. */
		Result() = default;
	};

	/** Destroys this instance of the Cullable class. */
	virtual ~Cullable();

	/** Called just before the instance is deleted to allow culling members. */
	virtual Result Cull();
};

/**
 * The CullList class is used to delete objects at the end of the main loop to
 * avoid problems with references to deleted pointers if an object were deleted
 * during execution.
 */
class CoreExport CullList final
{
	std::vector<Cullable*> list;
	std::vector<LocalUser*> SQlist;

public:
	/** Adds an item to the cull list
	 */
	void AddItem(Cullable* item) { list.push_back(item); }
	void AddSQItem(LocalUser* item) { SQlist.push_back(item); }

	/** Applies the cull list (deletes the contents)
	 */
	void Apply();
};

/** Represents an action which is executable by an action list */
class CoreExport ActionBase
	: public Cullable
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
