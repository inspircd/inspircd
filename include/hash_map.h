/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */
 
#ifndef INSPIRCD_HASHMAP_H
#define INSPIRCD_HASHMAP_H

#include "inspircd_config.h"

/** Where hash_map is varies from compiler to compiler
 * as it is not standard.
 */
#ifndef WIN32
#include <ext/hash_map>
/** Oddball linux namespace for hash_map */
#define nspace __gnu_cxx
#else
#include <hash_map>
#define nspace stdext
/** Oddball windows namespace for hash_map */
using stdext::hash_map;
#endif

#endif
