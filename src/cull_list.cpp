/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
	std::vector<LocalUser *> working;
	while (!SQlist.empty())
	{
		working.swap(SQlist);
		for(std::vector<LocalUser *>::iterator a = working.begin(); a != working.end(); a++)
		{
			LocalUser *u = *a;
			ServerInstance->SNO->WriteGlobalSno('a', "User %s SendQ exceeds connect class maximum of %lu",
				u->nick.c_str(), u->MyClass->GetSendqHardMax());
			ServerInstance->Users->QuitUser(u, "SendQ exceeded");
		}
		working.clear();
	}
	std::set<classbase*> gone;
	std::vector<classbase*> queue;
	queue.reserve(list.size() + 32);
	for(unsigned int i=0; i < list.size(); i++)
	{
		classbase* c = list[i];
		if (gone.insert(c).second)
		{
			ServerInstance->Logs->Log("CULLLIST", DEBUG, "Deleting %s @%p", typeid(*c).name(),
				(void*)c);
			c->cull();
			queue.push_back(c);
		}
		else
		{
			ServerInstance->Logs->Log("CULLLIST",DEBUG, "WARNING: Object @%p culled twice!",
				(void*)c);
		}
	}
	list.clear();
	for(unsigned int i=0; i < queue.size(); i++)
	{
		classbase* c = queue[i];
		delete c;
	}
	if (list.size())
	{
		ServerInstance->Logs->Log("CULLLIST",DEBUG, "WARNING: Objects added to cull list in a destructor");
		Apply();
	}
}

void ActionList::Run()
{
	for(unsigned int i=0; i < list.size(); i++)
	{
		list[i]->Call();
	}
	list.clear();
}
