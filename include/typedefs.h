/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2005, 2008 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef __TYPEDEF_H__
#define __TYPEDEF_H__

#if defined(WINDOWS) && !defined(HASHMAP_DEPRECATED)
	typedef nspace::hash_map<std::string, User*, nspace::hash_compare<std::string, std::less<std::string> > > user_hash;
	typedef nspace::hash_map<std::string, Channel*, nspace::hash_compare<std::string, std::less<std::string> > > chan_hash;
#else
	#ifdef HASHMAP_DEPRECATED
		typedef nspace::hash_map<std::string, User*, nspace::insensitive, irc::StrHashComp> user_hash;
		typedef nspace::hash_map<std::string, Channel*, nspace::insensitive, irc::StrHashComp> chan_hash;
	#else
		typedef nspace::hash_map<std::string, User*, nspace::hash<std::string>, irc::StrHashComp> user_hash;
		typedef nspace::hash_map<std::string, Channel*, nspace::hash<std::string>, irc::StrHashComp> chan_hash;
	#endif
#endif

/** Server name cache
 */
typedef std::vector<std::string*> servernamelist;

/** A cached text file stored line by line.
 */
typedef std::deque<std::string> file_cache;

#endif

