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

#ifndef __BANCACHE_H
#define __BANCACHE_H

#include <string>

class CoreExport BanCacheHit : public classbase
{
 private:
	InspIRCd *ServerInstance;
 public:
	std::string Type;
	std::string Reason;
	std::string IP;

	BanCacheHit(InspIRCd *Instance, const std::string &ip, const std::string &type, const std::string &reason)
	{
		ServerInstance = Instance;
		this->Type = type;
		this->Reason = reason;
		this->IP = ip;
	}
};

// must be defined after class BanCacheHit.
#ifndef WIN32
typedef nspace::hash_map<std::string, BanCacheHit *, nspace::hash<std::string> > BanCacheHash;
#else
typedef nspace::hash_map<std::string, BanCacheHit*, nspace::hash_compare<string, less<string> > > BanCacheHash;
#endif

class CoreExport BanCacheManager : public classbase
{
 private:
	BanCacheHash *BanHash;
	InspIRCd *ServerInstance;
 public:

	/** Creates and adds a Ban Cache item.
	 * @param ip The IP the item is for.
	 * @param type The type of ban cache item. std::string. .empty() means it's a negative match (user is allowed freely).
	 * @param reason The reason for the ban. Left .empty() if it's a negative match.
	 */
	BanCacheHit *AddHit(const std::string &ip, const std::string &type, const std::string &reason);
	BanCacheHit *GetHit(const std::string &ip);
	bool RemoveHit(BanCacheHit *b);

	/** Removes all entries of a given type, either positive or negative. Returns the number of hits removed.
	 * @param type The type of bancache entries to remove (e.g. 'G')
	 * @param positive Remove either positive (true) or negative (false) hits.
	 */
	int RemoveEntries(const std::string &type, bool positive);

	BanCacheManager(InspIRCd *Instance)
	{
		this->ServerInstance = Instance;
		this->BanHash = new BanCacheHash();
	}

	void RehashCache();
};

#endif
