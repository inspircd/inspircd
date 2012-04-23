/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
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
