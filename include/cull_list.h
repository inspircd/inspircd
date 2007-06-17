/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CULLLIST_H__
#define __CULLLIST_H__

// include the common header files

#include <string>
#include <deque>
#include <vector>
#include "users.h"
#include "channels.h"

class InspIRCd;

/** The CullItem class holds a user and their quitmessage,
 * and is used internally by the CullList class to compile
 * a list of users which are to be culled when a long
 * operation (such as a netsplit) has completed.
 */
class CoreExport CullItem : public classbase
{
 private:
	/** Holds a pointer to the user,
	 * must be valid and can be a local or remote user.
	 */
	userrec* user;
	/** Holds the quit reason to use for this user.
	 */
	std::string reason;
	/** Holds the quit reason opers see, if different from users
	 */
	std::string oper_reason;
	/** Silent items dont generate an snotice.
	 */
	bool silent;
 public:
	/** Constrcutor.
	* Initializes the CullItem with a user pointer
	* and their quit reason
	* @param u The user to add
	* @param r The quit reason of the added user
	* @param ro The quit reason to show to opers only
	*/
	CullItem(userrec* u, std::string &r, const char* ro = "");
	/** Constrcutor.
	 * Initializes the CullItem with a user pointer
	 * and their quit reason
	 * @param u The user to add
	 * @param r The quit reason of the added user
	 * @param ro The quit reason to show to opers only
	 */
	CullItem(userrec* u, const char* r, const char* ro = "");

	/** Make the quit silent a module is dealing with
	 * displaying this users quit, so we shouldn't
	 * send anything out.
	 */
	void MakeSilent();

	/** Returns true if the quit for this user has been set
	 * to silent.
	 */
	bool IsSilent();

	/** Destructor
	 */
	~CullItem();

	/** Returns a pointer to the user
	*/
	userrec* GetUser();
	/** Returns the user's quit reason
	*/
	std::string& GetReason();
	/** Returns oper reason
	 */
	std::string& GetOperReason();
};

/** The CullList class can be used by modules, and is used
 * by the core, to compile large lists of users in preperation
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
 */
class CoreExport CullList : public classbase
{
 private:
	/** Creator of this CullList
	 */
	InspIRCd* ServerInstance;

	/** Holds a list of users already added for quick lookup
	 */
	std::map<userrec*, userrec*> exempt;

	/** Holds a list of users being quit.
	 * See the information for CullItem for
	 * more information.
	 */
	std::vector<CullItem> list;

 public:
	/** Constructor.
	 * Clears the CullList::list and CullList::exempt
	 * items.
	 * @param Instance Creator of this CullList object
	 */
	CullList(InspIRCd* Instance);

	/** Adds a user to the cull list for later
	 * removal via QUIT.
	 * @param user The user to add
	 * @param reason The quit reason of the user being added
	 * @param o_reason The quit reason to show only to opers
	 */
	void AddItem(userrec* user, std::string &reason, const char* o_reason = "");

	/** Adds a user to the cull list for later
	 * removal via QUIT.
	 * @param user The user to add
	 * @param reason The quit reason of the user being added
	 * @param o_reason The quit reason to show only to opers
	 */
	void AddItem(userrec* user, const char* reason, const char* o_reason = "");

	/* Turn an item into a silent item (don't send out QUIT for this user)
	 */
	void MakeSilent(userrec* user);

	/** Applies the cull list, quitting all the users
	 * on the list with their quit reasons all at once.
	 * This is a very fast operation compared to
	 * iterating the user list and comparing each one,
	 * especially if there are multiple comparisons
	 * to be done, or recursion.
	 * @returns The number of users removed from IRC.
	 */
	int Apply();
};

#endif

