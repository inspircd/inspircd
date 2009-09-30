/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CULLLIST_H__
#define __CULLLIST_H__

/**
 * The CullList class is used to delete objects at the end of the main loop to
 * avoid problems with references to deleted pointers if an object were deleted
 * during execution.
 */
class CoreExport CullList
{
	std::vector<classbase*> list;

 public:
	/** Adds an item to the cull list
	 */
	void AddItem(classbase* item) { list.push_back(item); }

	/** Applies the cull list (deletes the contents)
	 */
	void Apply();
};

#endif

