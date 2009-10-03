/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

/** A cached text file stored line by line.
 */
typedef std::deque<std::string> file_cache;

#endif

