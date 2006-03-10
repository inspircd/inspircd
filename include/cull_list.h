/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CULLLIST_H__
#define __CULLLIST_H__

// include the common header files

#include <typeinfo>
#include <iostream>
#include <string>
#include <deque>
#include <sstream>
#include <vector>
#include "users.h"
#include "channels.h"

/** The CullItem class holds a user and their quitmessage,
 * and is used internally by the CullList class to compile
 * a list of users which are to be culled when a long
 * operation (such as a netsplit) has completed.
 */
class CullItem
{
 private:
	/** Holds a pointer to the user,
	 * must be valid and can be a local or remote user.
	 */
        userrec* user;
	/** Holds the quit reason to use for this user.
	 */
	std::string reason;
 public:
	/** Constrcutor.
	 * Initializes the CullItem with a user pointer
	 * and their quit reason
	 * @param u The user to add
	 * @param r The quit reason of the added user
	 */
        CullItem(userrec* u, std::string &r);
	CullItem(userrec* u, const char* r);

	~CullItem();

	/** Returns a pointer to the user
	 */
        userrec* GetUser();
	/** Returns the user's quit reason
	 */
	std::string& GetReason();
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
class CullList
{
 private:
	 /** Holds a list of users being quit.
	  * See the information for CullItem for
	  * more information.
	  */
         std::vector<CullItem> list;
	 /** A list of users who have already been
	  * placed on the list, as a map for fast
	  * reference. When deleting an item, the
	  * time_t value stored here must match
	  * the one of the actual userrec, otherwise
	  * we don't delete it (its a different user)
	  */
	 std::map<userrec*,time_t> exempt;
	 
	 /** Check if a user pointer is valid
	  * (e.g. it exists in the user hash)
	  */
	 bool IsValid(userrec* user);
 public:
	 /** Constructor.
	  * Clears the CullList::list and CullList::exempt
	  * items.
	  */
         CullList();
	 /** Adds a user to the cull list for later
	  * removal via QUIT.
	  * @param user The user to add
	  * @param reason The quit reason of the user being added
	  */
         void AddItem(userrec* user, std::string &reason);
	 void AddItem(userrec* user, const char* reason);
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
