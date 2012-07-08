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

BanCacheHit *BanCacheManager::AddHit(const std::string &ip, const std::string &type, const std::string &reason)
{
	BanCacheHit *b;

	if (this->BanHash->find(ip) != this->BanHash->end()) // can't have two cache entries on the same IP, sorry..
		return NULL;

	b = new BanCacheHit(ip, type, reason);

	this->BanHash->insert(std::make_pair(ip, b));
	return b;
}

BanCacheHit *BanCacheManager::AddHit(const std::string &ip, const std::string &type, const std::string &reason, time_t seconds)
{
	BanCacheHit *b;

	if (this->BanHash->find(ip) != this->BanHash->end()) // can't have two cache entries on the same IP, sorry..
		return NULL;

	b = new BanCacheHit(ip, type, reason, seconds);

	this->BanHash->insert(std::make_pair(ip, b));
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
			RemoveHit(i->second);
			return NULL; // out of date
		}

		return i->second; // hit.
	}
}

bool BanCacheManager::RemoveHit(BanCacheHit *b)
{
	BanCacheHash::iterator i;

	if (!b)
		return false; // I don't think so.

	i = this->BanHash->find(b->IP);

	if (i == this->BanHash->end())
	{
		// err..
		ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveHit(): I got asked to remove a hit that wasn't in the hash(?)");
	}
	else
	{
		this->BanHash->erase(i);
	}

	delete b;
	return true;
}

unsigned int BanCacheManager::RemoveEntries(const std::string &type, bool positive)
{
	int removed = 0;

	BanCacheHash::iterator safei;

	if (positive)
		ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveEntries(): Removing positive hits for " + type);
	else
		ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveEntries(): Removing negative hits for " + type);

	for (BanCacheHash::iterator n = BanHash->begin(); n != BanHash->end(); )
	{
		safei = n;
		safei++;

		BanCacheHit *b = n->second;

		/* Safe to delete items here through iterator 'n' */
		if (b->Type == type || !positive) // if removing negative hits, ignore type..
		{
			if ((positive && !b->Reason.empty()) || b->Reason.empty())
			{
				/* we need to remove this one. */
				ServerInstance->Logs->Log("BANCACHE", DEBUG, "BanCacheManager::RemoveEntries(): Removing a hit on " + b->IP);
				delete b;
				BanHash->erase(n); // WORD TO THE WISE: don't use RemoveHit here, because we MUST remove the iterator in a safe way.
				removed++;
			}
		}

		/* End of safe section */
		n = safei;
	}


	return removed;
}

void BanCacheManager::RehashCache()
{
	BanCacheHash* NewHash = new BanCacheHash();

	BanCacheHash::iterator safei;
	for (BanCacheHash::iterator n = BanHash->begin(); n != BanHash->end(); )
	{
		safei = n;
		safei++;

		/* Safe to delete items here through iterator 'n' */
		BanCacheHit *b = n->second;

		if (ServerInstance->Time() > b->Expiry)
		{
			/* we need to remove this one. */
			delete b;
			BanHash->erase(n); // WORD TO THE WISE: don't use RemoveHit here, because we MUST remove the iterator in a safe way.
		}
		else
		{
			/* Actually inserts a std::pair */
			NewHash->insert(*n);
		}

		/* End of safe section */

		n = safei;
	}

	delete BanHash;
	BanHash = NewHash;
}

BanCacheManager::~BanCacheManager()
{
	for (BanCacheHash::iterator n = BanHash->begin(); n != BanHash->end(); ++n)
		delete n->second;
	delete BanHash;
}
