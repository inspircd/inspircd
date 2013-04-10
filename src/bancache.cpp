/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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


/* $Core */

#include "inspircd.h"
#include "bancache.h"

BanCacheHit *BanCacheManager::AddHit(const std::string &ip, const std::string &type, const std::string &reason, time_t seconds)
{
	BanCacheHit*& b = (*BanHash)[ip];
	if (b != NULL) // can't have two cache entries on the same IP, sorry..
		return NULL;

	b = new BanCacheHit(type, reason, (seconds ? seconds : 86400));
	return b;
}

BanCacheHit *BanCacheManager::GetHit(const std::string &ip)
{
	BanCacheHash::iterator i = this->BanHash->find(ip);

	if (i == this->BanHash->end())
		return NULL; // free and safe
	else
	{
		if (ServerInstance->Time() > i->second->Expiry)
		{
			ServerInstance->Logs->Log("BANCACHE", DEBUG, "Hit on " + ip + " is out of date, removing!");
			delete i->second;
			BanHash->erase(i);
			return NULL; // out of date
		}

		return i->second; // hit.
	}
}

void BanCacheManager::RemoveEntries(const std::string& type, bool positive)
{
	if (positive)
		ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveEntries(): Removing positive hits for " + type);
	else
		ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveEntries(): Removing all negative hits");

	for (BanCacheHash::iterator i = BanHash->begin(); i != BanHash->end(); )
	{
		BanCacheHit* b = i->second;
		bool remove = false;

		if (positive)
		{
			// when removing positive hits, remove only if the type matches
			remove = b->IsPositive() && (b->Type == type);
		}
		else
		{
			// when removing negative hits, remove all of them
			remove = !b->IsPositive();
		}

		if (remove)
		{
			/* we need to remove this one. */
			ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveEntries(): Removing a hit on " + i->first);
			delete b;
			i = BanHash->erase(i);
		}
		else
			++i;
	}
}

BanCacheManager::~BanCacheManager()
{
	for (BanCacheHash::iterator n = BanHash->begin(); n != BanHash->end(); ++n)
		delete n->second;
	delete BanHash;
}
