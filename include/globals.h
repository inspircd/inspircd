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

typedef std::deque<std::string> file_cache;
typedef std::pair< std::string, std::string > KeyVal;
typedef std::vector< KeyVal > KeyValList;
typedef std::multimap< std::string, KeyValList > ConfigDataHash;

#endif
