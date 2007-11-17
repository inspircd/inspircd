/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDbancache */

#include "inspircd.h"
#include "bancache.h"

BanCacheHit *BanCacheManager::AddHit(const std::string &ip, const std::string &type, const std::string &reason)
{
	BanCacheHit *b;


	if (this->BanHash->find(ip) != this->BanHash->end()) // can't have two cache entries on the same IP, sorry..
		return NULL;

	b = new BanCacheHit(ServerInstance, ip, type, reason);

	this->BanHash->insert(std::make_pair(ip, b));
	return b;
}

BanCacheHit *BanCacheManager::GetHit(const std::string &ip)
{
	BanCacheHash::iterator i = this->BanHash->find(ip);

	if (i == this->BanHash->end())
		return NULL; // free and safe
	else
		return i->second; // hit.
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
		ServerInstance->Log(DEBUG, "BanCacheManager::RemoveHit(): I got asked to remove a hit that wasn't in the hash(?)");
	}
	else
	{
		this->BanHash->erase(i);
	}

	delete b;
	return true;
}

int BanCacheManager::RemoveEntries(const std::string &type, bool positive)
{
	int removed = 0;

	BanCacheHash::iterator safei;

	for (BanCacheHash::iterator n = BanHash->begin(); n != BanHash->end(); )
	{
		safei = n;
		safei++;

		BanCacheHit *b = n->second;

		/* Safe to delete items here through iterator 'n' */
		if (b->Type == type)
		{
			if ((positive && !b->Reason.empty()) || b->Reason.empty())
			{
				/* we need to remove this one. */
				delete b;
				b = NULL;
				BanHash->erase(n);
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

		/* Actually inserts a std::pair */
		NewHash->insert(*n);

		/* End of safe section */

		n = safei;
	}

	delete BanHash;
	BanHash = NewHash;
}
