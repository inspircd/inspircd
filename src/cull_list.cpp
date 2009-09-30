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

#include "inspircd.h"
#include <typeinfo>

void CullList::Apply()
{
	std::set<classbase*> gone;
	for(unsigned int i=0; i < list.size(); i++)
	{
		classbase* c = list[i];
		if (gone.insert(c).second)
		{
			ServerInstance->Logs->Log("CULLLIST", DEBUG, "Deleting %s @%p", typeid(*c).name(),
				(void*)c);
			c->cull();
			delete c;
		}
		else
		{
			ServerInstance->Logs->Log("CULLLIST",DEBUG, "WARNING: Object @%p culled twice!",
				(void*)c);
		}
	}
}

