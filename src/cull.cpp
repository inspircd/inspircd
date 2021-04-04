/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#ifdef INSPIRCD_ENABLE_RTTI
# include <typeinfo>
#endif

Cullable::Cullable()
{
#ifdef INSPIRCD_ENABLE_RTTI
	if (ServerInstance)
	{
		ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "Cullable::+%s @%p",
				typeid(*this).name(), static_cast<void*>(this));
	}
#endif
}

Cullable::~Cullable()
{
#ifdef INSPIRCD_ENABLE_RTTI
	if (ServerInstance)
	{
		ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "Cullable::~%s @%p",
			typeid(*this).name(), static_cast<void*>(this));
	}
#endif
}

Cullable::Result Cullable::Cull()
{
#ifdef INSPIRCD_ENABLE_RTTI
	if (ServerInstance)
	{
		ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "Cullable::-%s @%p",
			typeid(*this).name(), static_cast<void*>(this));
	}
#endif
	return Result();
}

void CullList::Apply()
{
	std::vector<LocalUser *> working;
	while (!SQlist.empty())
	{
		working.swap(SQlist);
		for(std::vector<LocalUser *>::iterator a = working.begin(); a != working.end(); a++)
		{
			LocalUser *u = *a;
			ServerInstance->SNO.WriteGlobalSno('a', "User %s SendQ exceeds connect class maximum of %lu",
				u->nick.c_str(), u->GetClass()->GetSendqHardMax());
			ServerInstance->Users.QuitUser(u, "SendQ exceeded");
		}
		working.clear();
	}
	std::set<Cullable*> gone;
	std::vector<Cullable*> queue;
	queue.reserve(list.size() + 32);
	for(unsigned int i=0; i < list.size(); i++)
	{
		Cullable* c = list[i];
		if (gone.insert(c).second)
		{
#ifdef INSPIRCD_ENABLE_RTTI
			ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "Deleting %s @%p", typeid(*c).name(),
				static_cast<void*>(c));
#else
			ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "Deleting @%p", static_cast<void*>(c));
#endif
			c->Cull();
			queue.push_back(c);
		}
		else
		{
			ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "WARNING: Object @%p culled twice!",
				static_cast<void*>(c));
		}
	}
	list.clear();
	for(unsigned int i=0; i < queue.size(); i++)
	{
		Cullable* c = queue[i];
		delete c;
	}
	if (!list.empty())
	{
		ServerInstance->Logs.Log("CULLLIST", LOG_DEBUG, "WARNING: Objects added to cull list in a destructor");
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
