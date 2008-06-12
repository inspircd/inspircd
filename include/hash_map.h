/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */
 
#ifndef INSPIRCD_HASHMAP_H
#define INSPIRCD_HASHMAP_H

/** Where hash_map is varies from compiler to compiler
 * as it is not standard.
 */
#ifndef WIN32
	#ifndef HASHMAP_DEPRECATED
		#include <ext/hash_map>
		/** Oddball linux namespace for hash_map */
		#define nspace __gnu_cxx
	#else
		#include <tr1/unordered_map>
		#define hash_map unordered_map
		#define nspace std::tr1
	#endif
#else
	#include <hash_map>
	#define nspace stdext
	/** Oddball windows namespace for hash_map */
	using stdext::hash_map;
#endif



#endif

