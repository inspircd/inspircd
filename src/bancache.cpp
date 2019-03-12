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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

BanCacheHit::BanCacheHit(const std::string& type, const std::string& reason, time_t seconds)
	: Type(type)
	, Reason(reason)
	, Expiry(ServerInstance->Time() + seconds)
{
}

BanCacheHit *BanCacheManager::AddHit(const std::string &ip, const std::string &type, const std::string &reason, time_t seconds)
{
	BanCacheHit*& b = BanHash[ip];
	if (b != NULL) // can't have two cache entries on the same IP, sorry..
		return NULL;

	b = new BanCacheHit(type, reason, (seconds ? seconds : 86400));
	return b;
}

BanCacheHit *BanCacheManager::GetHit(const std::string &ip)
{
	BanCacheHash::iterator i = this->BanHash.find(ip);

	if (i == this->BanHash.end())
		return NULL; // free and safe

	if (RemoveIfExpired(i))
		return NULL; // expired

	return i->second; // hit.
}

bool BanCacheManager::RemoveIfExpired(BanCacheHash::iterator& it)
{
	if (ServerInstance->Time() < it->second->Expiry)
		return false;

	ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "Hit on " + it->first + " is out of date, removing!");
	delete it->second;
	it = BanHash.erase(it);
	return true;
}

void BanCacheManager::RemoveEntries(const std::string& type, bool positive)
{
	if (positive)
		ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCacheManager::RemoveEntries(): Removing positive hits for " + type);
	else
		ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCacheManager::RemoveEntries(): Removing all negative hits");

	for (BanCacheHash::iterator i = BanHash.begin(); i != BanHash.end(); )
	{
		if (RemoveIfExpired(i))
			continue; // updates the iterator if expired

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
			ServerInstance->Logs->Log("BANCACHE", LOG_DEBUG, "BanCacheManager::RemoveEntries(): Removing a hit on " + i->first);
			delete b;
			i = BanHash.erase(i);
		}
		else
			++i;
	}
}

BanCacheManager::~BanCacheManager()
{
	for (BanCacheHash::iterator n = BanHash.begin(); n != BanHash.end(); ++n)
		delete n->second;
}
