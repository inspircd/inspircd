/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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

class CullItem
{
 private:
        userrec* user;
        std::string reason;
 public:
        CullItem(userrec* u, std::string r);
        userrec* GetUser();
        std::string GetReason();
};


class CullList
{
 private:
         std::vector<CullItem> list;
	 std::map<userrec*,int> exempt;
 public:
         CullList();
         void AddItem(userrec* user, std::string reason);
         int Apply();
};

#endif
