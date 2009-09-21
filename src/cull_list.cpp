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

/* $Core */

#include "inspircd.h"
#include "cull_list.h"

void CullList::Apply()
{
	std::vector<classbase*> todel(list.begin(), list.end());
	list.clear();
	for(std::vector<classbase*>::iterator i = todel.begin(); i != todel.end(); i++)
	{
		classbase* c = *i;
		c->cull();
		delete c;
	}
}

