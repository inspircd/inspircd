/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#ifndef __CULLLIST_H__
#define __CULLLIST_H__

/** The CullList class is used by the core to create lists of users
 * prior to actually quitting (and deleting the objects) all at once.
 * to quitting them all at once. This is faster than quitting
 * them within the loop, as the loops become tighter with
 * little or no comparisons within them. The CullList class
 * operates by allowing the programmer to push users onto
 * the list, each with a seperate quit reason, and then, once
 * the list is complete, call a method to flush the list,
 * quitting all the users upon it. A CullList may hold local
 * or remote users, but it may only hold each user once. If
 * you attempt to add the same user twice, then the second
 * attempt will be ignored.
 *
 * NOTE: Don't use this outside core, use the QuitUser method like everyone else!
 */
class CoreExport CullList : public classbase
{
 private:
	/** Creator of this CullList
	 */
	InspIRCd* ServerInstance;

	/** Holds a list of users being quit.
	 * See the information for CullItem for
	 * more information.
	 */
	std::vector<User *> list;
	std::vector<User *> SQlist;

 public:
	/** Constructor.
	 * @param Instance Creator of this CullList object
	 */
	CullList(InspIRCd* Instance);

	/** Adds a user to the cull list for later
	 * removal via QUIT.
	 * @param user The user to add
	 * @param reason The quit reason of the user being added
	 * @param o_reason The quit reason to show only to opers
	 */
	void AddItem(User* user);
	void AddSQItem(User* user);

	/* Turn an item into a silent item (don't send out QUIT for this user)
	 */
	void MakeSilent(User* user);

	/** Applies the cull list, quitting all the users
	 * on the list with their quit reasons all at once.
	 * This is a very fast operation compared to
	 * iterating the user list and comparing each one,
	 * especially if there are multiple comparisons
	 * to be done, or recursion.
	 */
	void Apply();
};

#endif

