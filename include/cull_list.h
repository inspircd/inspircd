/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

