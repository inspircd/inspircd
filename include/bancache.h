/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


#pragma once

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
	/** Time that the ban expires at
	 */
	time_t Expiry;

	BanCacheHit(const std::string& type, const std::string& reason, time_t seconds);

	bool IsPositive() const { return (!Reason.empty()); }
};

/** A manager for ban cache, which allocates and deallocates and checks cached bans.
 */
class CoreExport BanCacheManager
{
	/** A container of ban cache items.
	 */
	typedef TR1NS::unordered_map<std::string, BanCacheHit*, TR1NS::hash<std::string> > BanCacheHash;

	BanCacheHash BanHash;
	bool RemoveIfExpired(BanCacheHash::iterator& it);

 public:

	/** Creates and adds a Ban Cache item.
	 * @param ip The IP the item is for.
	 * @param type The type of ban cache item. std::string. .empty() means it's a negative match (user is allowed freely).
	 * @param reason The reason for the ban. Left .empty() if it's a negative match.
	 * @param seconds Number of seconds before nuking the bancache entry, the default is a day. This might seem long, but entries will be removed as G-lines/etc expire.
	 */
	BanCacheHit *AddHit(const std::string &ip, const std::string &type, const std::string &reason, time_t seconds = 0);
	BanCacheHit *GetHit(const std::string &ip);

	/** Removes all entries of a given type, either positive or negative. Returns the number of hits removed.
	 * @param type The type of bancache entries to remove (e.g. 'G')
	 * @param positive Remove either positive (true) or negative (false) hits.
	 */
	void RemoveEntries(const std::string& type, bool positive);

	~BanCacheManager();
};
