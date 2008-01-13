/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDusermanager */

#include "inspircd.h"

void UserManager::AddLocalClone(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
		x->second++;
	else
		local_clones[user->GetIPString()] = 1;
}

void UserManager::AddGlobalClone(User *user)
{
	clonemap::iterator y = global_clones.find(user->GetIPString());
	if (y != global_clones.end())
		y->second++;
	else
		global_clones[user->GetIPString()] = 1;
}

void UserManager::RemoveCloneCounts(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
	{
		x->second--;
		if (!x->second)
		{
			local_clones.erase(x);
		}
	}
	
	clonemap::iterator y = global_clones.find(user->GetIPString());
	if (y != global_clones.end())
	{
		y->second--;
		if (!y->second)
		{
			global_clones.erase(y);
		}
	}
}

unsigned long UserManager::GlobalCloneCount(User *user)
{
	clonemap::iterator x = global_clones.find(user->GetIPString());
	if (x != global_clones.end())
		return x->second;
	else
		return 0;
}

unsigned long UserManager::LocalCloneCount(User *user)
{
	clonemap::iterator x = local_clones.find(user->GetIPString());
	if (x != local_clones.end())
		return x->second;
	else
		return 0;
}
