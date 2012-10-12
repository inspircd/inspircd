/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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


#ifndef INSPIRCD_HASHMAP_H
#define INSPIRCD_HASHMAP_H

#include "inspircd_config.h"

	/** Where hash_map is varies from compiler to compiler
	 * as it is not standard unless we have tr1.
	 */
	#ifndef _WIN32
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
		#include <unordered_map>
		#define HAS_TR1_UNORDERED
		#define HASHMAP_DEPRECATED
	#endif

	// tr1: restoring sanity to our headers. now if only compiler vendors could agree on a FUCKING INCLUDE FILE.
	#ifdef HAS_TR1_UNORDERED
		#define hash_map unordered_map
		#define nspace std::tr1
		#define BEGIN_HASHMAP_NAMESPACE namespace std { namespace tr1 {
		#define END_HASHMAP_NAMESPACE } }
	#endif

#endif
