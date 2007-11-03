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
 public:
	const std::string Type;
	const std::string Reason;
	const bool Banned;
	const time_t Duration;
	const time_t Creation;
};

// must be defined after class BanCacheHit.
typedef nspace::hash_map<std::string, BanCacheHit *, nspace::hash<std::string> > BanCacheHash;

class CoreExport BanCacheManager : public classbase
{
 private:
	BanCacheHash *BanHash;
	InspIRCd *ServerInstance;
 public:
	BanCacheHit *AddHit(const std::string &ip, bool banned, const std::string &reason);
	BanCacheHit *GetHit(const std::string &ip);

	BanCacheManager(InspIRCd *Instance)
	{
		this->ServerInstance = Instance;
		this->BanHash = new BanCacheHash();
	}
};

#endif
