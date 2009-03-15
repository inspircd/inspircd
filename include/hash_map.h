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

#ifndef INSPIRCD_HASHMAP_H
#define INSPIRCD_HASHMAP_H

/** Where hash_map is varies from compiler to compiler
 * as it is not standard unless we have tr1.
 */
#ifndef WIN32
	#ifndef HASHMAP_DEPRECATED
		#include <ext/hash_map>
		/** Oddball linux namespace for hash_map */
		#define nspace __gnu_cxx
		#define BEGIN_HASHMAP_NAMESPACE namespace nspace {
		#define END_HASHMAP_NAMESPACE }
	#else
		/** Yay, we have tr1! */
		#include <tr1/unordered_map>
		/** Not so oddball linux namespace for hash_map with gcc 4.0 and above */
		#define hash_map unordered_map
		#define nspace std::tr1
		#define BEGIN_HASHMAP_NAMESPACE namespace std { namespace tr1 {
		#define END_HASHMAP_NAMESPACE } }
	#endif
#else
	#include <hash_map>
	#define nspace stdext
	/** Oddball windows namespace for hash_map */
	using stdext::hash_map;
	#define BEGIN_HASHMAP_NAMESPACE namespace nspace {
	#define END_HASHMAP_NAMESPACE }
#endif

#endif
