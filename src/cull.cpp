/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
		ServerInstance->Logs.Debug("CULLLIST", "Cullable::+{} @{}",
			typeid(*this).name(), fmt::ptr(this));
	}
#endif
}

Cullable::~Cullable()
{
#ifdef INSPIRCD_ENABLE_RTTI
	if (ServerInstance)
	{
		ServerInstance->Logs.Debug("CULLLIST", "Cullable::~{} @{}",
			typeid(*this).name(), fmt::ptr(this));
	}
#endif
}

Cullable::Result Cullable::Cull()
{
#ifdef INSPIRCD_ENABLE_RTTI
	if (ServerInstance)
	{
		ServerInstance->Logs.Debug("CULLLIST", "Cullable::-{} @{}",
			typeid(*this).name(), fmt::ptr(this));
	}
#endif
	return {};
}

void CullList::Apply()
{
	std::vector<LocalUser *> working;
	while (!SQlist.empty())
	{
		working.swap(SQlist);
		for (auto* u : working)
		{
			ServerInstance->SNO.WriteGlobalSno('a', "User {} SendQ exceeds connect class maximum of {}",
				u->nick, u->GetClass()->hardsendqmax);
			ServerInstance->Users.QuitUser(u, "SendQ exceeded");
		}
		working.clear();
	}

	std::set<Cullable*> gone;
	std::vector<Cullable*> queue;
	queue.reserve(list.size() + 32);
	for (auto* c : list)
	{
		if (gone.insert(c).second)
		{
#ifdef INSPIRCD_ENABLE_RTTI
			ServerInstance->Logs.Debug("CULLLIST", "Deleting {} @{}", typeid(*c).name(),
				fmt::ptr(c));
#endif
			c->Cull();
			queue.push_back(c);
		}
		else
		{
#ifdef INSPIRCD_ENABLE_RTTI
			ServerInstance->Logs.Debug("CULLLIST", "WARNING: Object {} @{} culled twice!",
				typeid(*c).name(), fmt::ptr(c));
#else
			ServerInstance->Logs.Debug("CULLLIST", "WARNING: Object @{} culled twice!",
				fmt::ptr(c));
#endif
		}
	}
	list.clear();

	for (auto* c : queue)
		delete c;

	if (!list.empty())
	{
		ServerInstance->Logs.Debug("CULLLIST", "WARNING: %zu objects added to the cull list from a destructor", list.size());
		Apply();
	}
}

void ActionList::Run()
{
	for (auto* action : list)
		action->Call();
	list.clear();
}
