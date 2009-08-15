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
		#ifdef HASHMAP_DEPRECATED
			// GCC4+ has deprecated hash_map and uses tr1. But of course, uses a different include to MSVC. FOR FUCKS SAKE.
			#include <tr1/unordered_map>
			#define HAS_TR1_UNORDERED
		#else
			#include <ext/hash_map>
			/** Oddball linux namespace for hash_map */
			#define nspace __gnu_cxx
			#define BEGIN_HASHMAP_NAMESPACE namespace nspace {
			#define END_HASHMAP_NAMESPACE }
		#endif
	#else
		#if _MSC_VER >= 1600
			// New MSVC has tr1. Just to make things fucked up, though, MSVC and GCC use different includes! FFS.
			#include <unordered_map>
			#define HAS_TR1_UNORDERED
			#define HASHMAP_DEPRECATED
		#else
			#define nspace stdext
			/** Oddball windows namespace for hash_map */
			using stdext::hash_map;
			#define BEGIN_HASHMAP_NAMESPACE namespace nspace {
			#define END_HASHMAP_NAMESPACE }
		#endif
	#endif

	// tr1: restoring sanity to our headers. now if only compiler vendors could agree on a FUCKING INCLUDE FILE.
	#ifdef HAS_TR1_UNORDERED
		#define hash_map unordered_map
		#define nspace std::tr1
		#define BEGIN_HASHMAP_NAMESPACE namespace std { namespace tr1 {
		#define END_HASHMAP_NAMESPACE } }
	#endif

#endif
