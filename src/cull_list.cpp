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

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "inspircd.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "typedefs.h"
#include "cull_list.h"

extern InspIRCd* ServerInstance;
extern user_hash clientlist;

bool CullList::IsValid(userrec* user)
{
	for (user_hash::iterator u = clientlist.begin(); u != clientlist.end(); u++)
	{
		userrec* u2 = (userrec*)*u;
		if (user == u2)
			return true;
	}
	return false;
}

CullItem::CullItem(userrec* u, std::string r)
{
        this->user = u;
        this->reason = r;
}

userrec* CullItem::GetUser()
{
        return this->user;
}

std::string CullItem::GetReason()
{
        return this->reason;
}

CullList::CullList()
{
	list.clear();
        exempt.clear();
}

void CullList::AddItem(userrec* user, std::string reason)
{
	if (exempt.find(user) == exempt.end())
	{
	        CullItem item(user,reason);
	        list.push_back(item);
		names.push_back(user->nick);
	        exempt[user] = 1;
	}
}

int CullList::Apply()
{
        int n = 0;
        while (list.size())
        {
		std::vector<CullItem>::iterator a = list.begin();
		userrec* u = a->GetUser();
		/* Because ServerInstance->DoOneIteration can
		 * take the user away from us in the middle of
		 * our operation, we should check to see if this
		 * pointer is still valid by iterating the hash.
		 * It's expensive, yes, but the DoOneIteration
		 * call stops it being horrendously bad.
		 */
		if (IsValid(u))
		{
			kill_link(u,a->GetReason().c_str());
			list.erase(list.begin());
			/* So that huge numbers of quits dont block,
			 * we yield back to our mainloop every 15
			 * iterations.
			 * The DoOneIteration call basically acts
			 * like a software threading mechanism.
			 */
			if (((n++) % 15) == 0)
			{
				ServerInstance->DoOneIteration(false);
			}
		}
        }
        return n;
}
