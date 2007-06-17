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

#ifndef __WORLD_H
#define __WORLD_H

#include <string>
#include <deque>
#include <map>
#include <vector>

/** A cached text file stored with its contents as lines
 */
typedef std::deque<std::string> file_cache;

/** A configuration key and value pair
 */
typedef std::pair< std::string, std::string > KeyVal;

/** A list of related configuration keys and values
 */
typedef std::vector< KeyVal > KeyValList;

/** An entire config file, built up of KeyValLists
 */
typedef std::multimap< std::string, KeyValList > ConfigDataHash;

#endif

