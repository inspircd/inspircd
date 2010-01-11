/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __BANCACHE_H
#define __BANCACHE_H

/** Stores a cached ban entry.
 * Each ban has one of these hashed in a hash_map to make for faster removal
 * of already-banned users in the case that they try to reconnect. As no wildcard
 * matching is done on these IPs, the speed of the system is improved. These cache
 * entries expire every few hours, which is a reasonable expiry for any reasonable
 * sized network.
 */
class CoreExport BanCacheHit
{
 public:
	/** Type of cached ban
	 */
	std::string Type;
	/** Reason, shown as quit message
	 */
	std::string Reason;
	/** IP to match against, no wildcards here (of course)
	 */
	std::string IP;
	/** Time that the ban expires at
	 */
	time_t Expiry;

	BanCacheHit(const std::string &ip, const std::string &type, const std::string &reason)
	{
		this->Type = type;
		this->Reason = reason;
		this->IP = ip;
		this->Expiry = ServerInstance->Time() + 86400; // a day. this might seem long, but entries will be removed as glines/etc expire.
	}

	// overridden to allow custom time
	BanCacheHit(const std::string &ip, const std::string &type, const std::string &reason, time_t seconds)
	{
		this->Type = type;
		this->Reason = reason;
		this->IP = ip;
		this->Expiry = ServerInstance->Time() + seconds;
	}
};

/* A container of ban cache items.
 * must be defined after class BanCacheHit.
 */
#if defined(WINDOWS) && !defined(HASHMAP_DEPRECATED)
typedef nspace::hash_map<std::string, BanCacheHit*, nspace::hash_compare<std::string, std::less<std::string> > > BanCacheHash;
#else
typedef nspace::hash_map<std::string, BanCacheHit*, nspace::hash<std::string> > BanCacheHash;
#endif

/** A manager for ban cache, which allocates and deallocates and checks cached bans.
 */
class CoreExport BanCacheManager
{
 private:
	BanCacheHash* BanHash;
 public:

	/** Creates and adds a Ban Cache item.
	 * @param ip The IP the item is for.
	 * @param type The type of ban cache item. std::string. .empty() means it's a negative match (user is allowed freely).
	 * @param reason The reason for the ban. Left .empty() if it's a negative match.
	 */
	BanCacheHit *AddHit(const std::string &ip, const std::string &type, const std::string &reason);

	// Overridden to allow an optional number of seconds before expiry
	BanCacheHit *AddHit(const std::string &ip, const std::string &type, const std::string &reason, time_t seconds);
	BanCacheHit *GetHit(const std::string &ip);
	bool RemoveHit(BanCacheHit *b);

	/** Removes all entries of a given type, either positive or negative. Returns the number of hits removed.
	 * @param type The type of bancache entries to remove (e.g. 'G')
	 * @param positive Remove either positive (true) or negative (false) hits.
	 */
	unsigned int RemoveEntries(const std::string &type, bool positive);

	BanCacheManager()
	{
		this->BanHash = new BanCacheHash();
	}
	~BanCacheManager();
	void RehashCache();
};

#endif
