/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef CULLLIST_H
#define CULLLIST_H

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

class CoreExport ActionList
{
	std::vector<HandlerBase0<void>*> list;

 public:
	/** Adds an item to the list
	 */
	void AddAction(HandlerBase0<void>* item) { list.push_back(item); }

	/** Runs the items
	 */
	void Run();

};

#endif

